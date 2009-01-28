#ifdef HAVE_CONFIG_H
#include "thrucommon_config.h"
#endif
/* hack to work around thrift installing config.h's */
#undef HAVE_CONFIG_H

#if HAVE_LIBSPREAD

#include "ThruLogging.h"
#include "Spread.h"
#include <stdlib.h>

using namespace std;

string SP_error_to_string (int error);


/* TODO:
 *  - thread safe queuing of messages
 *  - multi-send, and test multi-receive
 *  - poll based/timeout so that threads calling run can die
 *  - implement circuit breaker pattern around spread...
 */

Spread::Spread (const string & name, const string & private_name)
{

    T_DEBUG("Spread: name=%s, private_name=%s",
             name.c_str (), private_name.c_str ());


    this->name = name;
    this->private_name = private_name;

    // connection
    char private_group[MAX_GROUP_NAME];
    int ret = SP_connect (this->name.c_str (),
                          this->private_name.c_str (), 0, 1,
                          &this->mbox, private_group);
    if (ret < 0)
    {
        string error = SP_error_to_string (ret);
        T_ERROR ( error.c_str());
        throw new SpreadException (error);
    }

    // save our private group name
    this->private_group = private_group;
    T_DEBUG ( "Spread: private_group=%s",this->private_group);
}

Spread::~Spread ()
{

    map<string, set<string> >::iterator i;
    for (i = this->groups.begin ();
         i != this->groups.end ();
         i++)
    {
        SP_leave (this->mbox, (*i).first.c_str ());
    }
    SP_disconnect (this->mbox);
}

void Spread::join (const std::string & group)
{
    T_DEBUG ("join: group=%s", group.c_str());
    map<string, set<string> >::iterator i;
    i = this->groups.find (group.c_str ());
    if (i != this->groups.end ())
    {
        T_DEBUG ("    already a member");
    }
    else
    {
        int ret = SP_join (this->mbox, group.c_str ());
        if (ret < 0)
        {
            string error = SP_error_to_string (ret);
            T_ERROR (error.c_str());
            throw new SpreadException (error);
        }
        // this will create the groups key in the map, but we'll have to wait
        // for a membership message before we'll have the list of people
        // that's handled elsewhere, as are membership changes
        this->groups[group];
        T_DEBUG ("    joined");
    }
}

void Spread::leave (const std::string & group)
{
    T_DEBUG ("leave: group=%s", group.c_str());
    map<string, set<string> >::iterator i;
    i = this->groups.find (group.c_str ());
    if (i != this->groups.end ())
    {
        SP_leave (this->mbox, group.c_str ());
        this->groups.erase (i);
        T_DEBUG ( "    left");
    }
}

void Spread::subscribe (const string & sender, const string & group,
                        const int message_type,
                        SubscriberCallbackInfo * callback_info)
{

    T_DEBUG("subscribe: sender=%s, group=%s, message_type=%d",
             sender.c_str (), group.c_str (), message_type);

    if (!group.empty () && group.find ("#") == string::npos)
    {
        // make sure we're a member of the group in question
        this->join (group);
    }
    // and subscribe the caller up
    subscriptions[group][message_type][sender].push_back (callback_info);
}

void Spread::send (const service service_type, const string & group,
                   const int message_type, const char * message,
                   const int message_len)
{
    T_DEBUG( "send: group=%s, message_type=%d, message_len=%d",
             group.c_str (), message_type, message_len);

    int ret = SP_multicast (this->mbox, service_type, group.c_str (),
                            message_type, message_len, message);
    if (ret < 0)
    {
        string error = SP_error_to_string (ret);
        T_ERROR (error.c_str());
        throw new SpreadException (error);
    }
}


// message will be copied in to a local buffer
void Spread::queue (const service service_type, const string & group,
                    const int message_type, const char * message,
                    const int message_len)
{

    T_DEBUG( "queue: group=%s, message_type=%d", group.c_str (),
             message_type);

    QueuedMessage * queued_message =
        (QueuedMessage*)malloc (sizeof (QueuedMessage));
    queued_message->service_type = service_type;
    queued_message->group = (char *)malloc (sizeof (char) * group.length ());
    memcpy(queued_message->group, group.c_str (), group.length ());
    queued_message->message_type = message_type;
    queued_message->message = (char *)malloc (sizeof (char) * message_len);
    memcpy(queued_message->message, message, message_len);
    queued_message->message_len = message_len;
    this->pending_messages.push (queued_message);
}

void Spread::run (int count)
{

    T_DEBUG("run: count=%d", count);


    service service_type = 0;
    char sender[MAX_GROUP_NAME];
    char max_groups = 2;
    int num_groups;
    int16_t type = 0;
    int endian_mismatch;
    int buf_size = 1024;
    int buf_len;

    int i = 0;
    while (!count || i < count)
    {
        // drain any pending messages
        this->drain_pending ();

        // created here so that if we have to grow them they'll be recreated
        // larger, see errorhandling below
        char groups[max_groups][MAX_GROUP_NAME];
        char buf[buf_size];

        T_DEBUG ("run:    receiving");
        buf_len = SP_receive (this->mbox, &service_type, sender,
                              max_groups, &num_groups, groups, &type,
                              &endian_mismatch, buf_size, buf);
        if (buf_len > 0)
        {
            if (Is_regular_mess (service_type))
            {
                // can't really hurt since we don't change the size, only
                // useful when plain text is being sent
                if (buf_len < buf_size)
                    buf[buf_len] = '\0';
                // convert groups array to a vector
                vector<string> group_strs;
                for (int n = 0; n < num_groups; n++)
                    group_strs.push_back (groups[n]);
                // and dispatch
                this->dispatch (sender, group_strs, type, buf, buf_len);
                // count it as handled
                i++;
            }
            else if (Is_reg_memb_mess (service_type))
            {
                membership_info  memb_info;
                int ret = SP_get_memb_info (buf, service_type, &memb_info);
                if (ret < 0)
                {
                    string error = SP_error_to_string (ret);
                    T_ERROR (error.c_str());
                    throw new SpreadException (error);
                }
                else if (Is_caused_join_mess (service_type))
                {
                    T_INFO("run: new member=%s", memb_info.changed_member);
                }
                else if (Is_caused_leave_mess (service_type) ||
                         Is_caused_disconnect_mess (service_type))
                {
                    T_INFO ("run: leaving member=%s",memb_info.changed_member);
                }
                else if (Is_caused_network_mess (service_type))
                {
                    T_INFO ("run: network change...");
                }

                this->groups[sender].clear ();
                T_DEBUG ("run: membership group=%s", sender.c_str());
                for (int j = 0; j < num_groups; j++)
                {
                    this->groups[sender].insert (groups[j]);
                    T_DEBUG ("run:    member=%s",groups[j].c_str());
                }
            }
        }
        else
        {
            switch (buf_len)
            {
                case ILLEGAL_SESSION:
                    // ILLEGAL_SESSION - The mbox given to receive on was illegal.
                    throw new SpreadException ("replication: spread error ILLEGAL_SESSION, stopping");
                    break;
                case ILLEGAL_MESSAGE:
                    // ILLEGAL_MESSAGE - The message had an illegal structure, like a scatter not filled out correctly.
                    throw new SpreadException ("replication: spread error ILLEGAL_MESSAGE, ignoring");
                    break;
                case CONNECTION_CLOSED:
                    // CONNECTION_CLOSED - During  communication  to  receive  the  message  communication errors occured and the receive could not be completed.
                    throw new SpreadException ("replication: spread error CONNECTION_CLOSED");
                    // TODO: try and reconnect
                    break;
                case GROUPS_TOO_SHORT:
                    // GROUPS_TOO_SHORT - If the groups array is too short to hold  the  entire  list  of groups this message was sent to then this error is returned and the num_groups field will be set to the negative of the  number of groups needed.
                    max_groups = -num_groups;
                    break;
                case BUFFER_TOO_SHORT:
                    // BUFFER_TOO_SHORT - If  the  message body buffer mess is too short to hold the mes‐ sage being  received  then  this  error  is  returned  and  the endian_mismatch  field  is  set  to  the  negative value of the required buffer length.
                    buf_size = -endian_mismatch;
                    break;
            }
        }
    }
    T_DEBUG ("run:    done");
}

void Spread::make_callbacks (vector<SubscriberCallbackInfo *> & callbacks,
                             const string & sender,
                             const vector<string> & groups,
                             const int message_type, const char * message,
                             const int message_len)
{

    T_DEBUG("make_callbacks: callbacks.size=%d, sender=%s, group[0]=%s, groups.size=%d, message_type=%d",
            (int)callbacks.size (), sender.c_str (), groups[0].c_str (),
            (int)groups.size (), message_type);


    bool ret;
    vector<SubscriberCallbackInfo *>::iterator i;
    for (i = callbacks.begin (); i < callbacks.end (); i++)
    {
        T_DEBUG ("make_callbacks:    callback");
        SubscriberCallbackInfo * callback_info = (*i);
        // TODO: nulls?
        ret = (callback_info->callback)(this, sender, groups, message_type,
                                        message, message_len,
                                        callback_info->data);
        if (!ret)
        {
            T_DEBUG ("make_callbacks:    removing callback");
            // TODO: what about freeing/deleting?
            callbacks.erase (i);
        }
    }

    T_DEBUG("make_callbacks: done, callbacks.size=%d",
                 (int)callbacks.size ());

}

void Spread::dispatch (const string & sender, const vector<string> & groups,
                       const int message_type, const char * message,
                       const int message_len)
{

    T_DEBUG("dispatch: sender=%s, groups[0]=%s, group.size=%d, message_type=%d",
            sender.c_str (), groups[0].c_str (), (int)groups.size(),
            message_type);

    // if we haven't installed any callbacks, there's no reason to continue
    if (this->subscriptions.empty ())
        return;

    // holy fuck this is a mess, this could be used as a proof of how shitty
    // C++ is compared to perl where this is 8 lines of code :(

    for (size_t i = 0; i < groups.size (); i++)
    {
        T_DEBUG ("dispatch:    groups[i]=%s", groups[i].c_str());
        map<string, map<int, map<string,
            vector<SubscriberCallbackInfo *> > > >::iterator g =
                this->subscriptions.find (groups[i]);
        if (g != this->subscriptions.end ())
        {
            map<int, map<string,
                vector<SubscriberCallbackInfo *> > >::iterator g_t =
                    (*g).second.find (message_type);
            if (g_t != (*g).second.end ())
            {
                map<string, vector<SubscriberCallbackInfo *> >::iterator g_t_s =
                    (*g_t).second.find (sender);
                if (g_t_s != (*g_t).second.end ())
                {
                    // we have a sender, group, type match
                    T_DEBUG ("dispatch:    g_t_s");
                    this->make_callbacks ((*g_t_s).second, sender, groups,
                                          message_type, message, message_len);
                }
                map<string, vector<SubscriberCallbackInfo *> >::iterator g_t_es =
                    (*g_t).second.find ("");
                if (g_t_es != (*g_t).second.end ())
                {
                    // we have a sender, group, empty type match
                    T_DEBUG ("dispatch:    g_t_es");
                    this->make_callbacks ((*g_t_es).second, sender, groups,
                                          message_type, message, message_len);
                }
            }
            map<int, map<string,
                vector<SubscriberCallbackInfo *> > >::iterator g_et =
                    (*g).second.find (-1);
            if (g_et != (*g).second.end ())
            {
                map<string, vector<SubscriberCallbackInfo *> >::iterator g_et_s =
                    (*g_et).second.find (sender);
                if (g_et_s != (*g_et).second.end ())
                {
                    // we have a sender, empty group, type match
                    T_DEBUG ("dispatch:    g_et_s");
                    this->make_callbacks ((*g_et_s).second, sender, groups,
                                          message_type, message, message_len);
                }
                map<string, vector<SubscriberCallbackInfo *> >::iterator g_et_es =
                    (*g_et).second.find ("");
                if (g_et_es != (*g_et).second.end ())
                {
                    // we have a sender, empty group, empty type match
                    T_DEBUG ("dispatch:    g_et_es");
                    this->make_callbacks ((*g_et_es).second, sender, groups,
                                          message_type, message, message_len);
                }
            }
        }
    }
    map<string, map<int, map<string,
        vector<SubscriberCallbackInfo *> > > >::iterator
            eg = this->subscriptions.find ("");
    if (eg != this->subscriptions.end ())
    {
        map<int, map<string,
            vector<SubscriberCallbackInfo *> > >::iterator eg_t =
                (*eg).second.find (message_type);
        if (eg_t != (*eg).second.end ())
        {
            map<string, vector<SubscriberCallbackInfo *> >::iterator eg_t_s =
                (*eg_t).second.find (sender);
            if (eg_t_s != (*eg_t).second.end ())
            {
                // we have a empty sender, group, type match
                T_DEBUG ("dispatch:    eg_t_s");
                this->make_callbacks ((*eg_t_s).second, sender, groups,
                                      message_type, message, message_len);
            }
            map<string, vector<SubscriberCallbackInfo *> >::iterator eg_t_es =
                (*eg_t).second.find ("");
            if (eg_t_es != (*eg_t).second.end ())
            {
                // we have a empty sender, group, empty type match
                T_DEBUG ("dispatch:    eg_t_es");
                this->make_callbacks ((*eg_t_es).second, sender, groups,
                                      message_type, message, message_len);
            }
        }
        map<int, map<string,
            vector<SubscriberCallbackInfo *> > >::iterator eg_et =
                (*eg).second.find (-1);
        if (eg_et != (*eg).second.end ())
        {
            map<string, vector<SubscriberCallbackInfo *> >::iterator eg_et_s =
                (*eg_et).second.find (sender);
            if (eg_et_s != (*eg_et).second.end ())
            {
                // we have a empty sender, empty group, type match
                T_DEBUG ("dispatch:    eg_et_s");
                this->make_callbacks ((*eg_et_s).second, sender, groups,
                                      message_type, message, message_len);
            }
            map<string, vector<SubscriberCallbackInfo *> >::iterator eg_et_es =
                (*eg_et).second.find ("");
            if (eg_et_es != (*eg_et).second.end ())
            {
                // we have a empty sender, empty group, empty type match
                T_DEBUG ("dispatch:    eg_et_es");
                this->make_callbacks ((*eg_et_es).second, sender, groups,
                                      message_type, message, message_len);
            }
        }
    }
}

void Spread::drain_pending ()
{
    T_DEBUG("drain_pending: pending_messages.size=%d",
            (int)this->pending_messages.size ());

    QueuedMessage * qm;
    while (!this->pending_messages.empty ())
    {
        T_DEBUG ("drain_pending:    draining");
        qm = this->pending_messages.front ();
        this->pending_messages.pop ();
        this->send (qm->service_type, qm->group, qm->message_type,
                    qm->message, qm->message_len);
        free (qm->group);
        free (qm->message);
        free (qm);
    }
    T_DEBUG ("drain_pending: done");
}

// copied from sp.c, redic that it's a function that aborts in a library rather
// than returning the error string to you to do something with, like log it
// maybe.
string SP_error_to_string (int error)
{
    // convert int to string
    char buf[10];
    sprintf (buf, "%d", error);
    string error_str (buf);
    // then wrap it in the error
    switch (error)
    {
        case ILLEGAL_SPREAD:
            return "SP_error: (" + error_str + ") Illegal spread was provided";
        case COULD_NOT_CONNECT:
            return "SP_error: (" + error_str + ") Could not connect. Is Spread running?";
        case REJECT_QUOTA:
            return "SP_error: (" + error_str + ") Connection rejected, to many users";
        case REJECT_NO_NAME:
            return "SP_error: (" + error_str + ") Connection rejected, no name was supplied";
        case REJECT_ILLEGAL_NAME:
            return "SP_error: (" + error_str + ") Connection rejected, illegal name";
        case REJECT_NOT_UNIQUE:
            return "SP_error: (" + error_str + ") Connection rejected, name not unique";
        case REJECT_VERSION:
            return "SP_error: (" + error_str + ") Connection rejected, library does not fit daemon";
        case CONNECTION_CLOSED:
            return "SP_error: (" + error_str + ") Connection closed by spread";
        case REJECT_AUTH:
            return "SP_error: (" + error_str + ") Connection rejected, authentication failed";
        case ILLEGAL_SESSION:
            return "SP_error: (" + error_str + ") Illegal session was supplied";
        case ILLEGAL_SERVICE:
            return "SP_error: (" + error_str + ") Illegal service request";
        case ILLEGAL_MESSAGE:
            return "SP_error: (" + error_str + ") Illegal message";
        case ILLEGAL_GROUP:
            return "SP_error: (" + error_str + ") Illegal group";
        case BUFFER_TOO_SHORT:
            return "SP_error: (" + error_str + ") The supplied buffer was too short";
        case GROUPS_TOO_SHORT:
            return "SP_error: (" + error_str + ") The supplied groups list was too short";
        case MESSAGE_TOO_LONG:
            return "SP_error: (" + error_str + ") The message body + group names was too large to fit in a message";
        case NET_ERROR_ON_SESSION:
            return "SP_error: (" + error_str + ") The network socket experienced an error. This Spread mailbox will no longer work until the connection is disconnected and then reconnected";
        default:
            return "SP_error: (" + error_str + ") unrecognized error";
    }
}

#endif /* HAVE_LIBSPREAD */
