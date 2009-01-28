#ifdef HAVE_CONFIG_H
#include "thrucommon_config.h"
#endif
/* hack to work around thrift and log4cxx installing config.h's */
#undef HAVE_CONFIG_H

#if HAVE_LIBSPREAD && HAVE_LIBUUID

#include "ReplicationRecorder.h"
#include "ThruLogging.h"

#include <protocol/TBinaryProtocol.h>
#include <transport/TTransportUtils.h>
#include <stdlib.h>

#define ORIG_MESSAGE_TYPE 1
#define REPLAY_MESSAGE_TYPE 101

using namespace boost;
using namespace facebook::thrift::protocol;
using namespace facebook::thrift::transport;
using namespace std;

struct RecordedMessage
{
    Message * message;
    char * buf;
    int len;
};

string SP_error_to_string (int error);


ReplicationRecorder::ReplicationRecorder (const string & replication_name,
                                          const string & replication_private_name,
                                          const string & replication_group) :
    spread (replication_name, replication_private_name),
    replication_group (replication_group)
{
    T_DEBUG ("ReplicationRecorder:");

    // subscribe to "live" messages
    SubscriberCallbackInfo * live_callback_info = new SubscriberCallbackInfo ();
    live_callback_info->callback = &orig_message_callback;
    live_callback_info->data = this;
    this->spread.subscribe ("", this->replication_group, ORIG_MESSAGE_TYPE,
                            live_callback_info);

    // subscribe to "replay" messages, both broadcast and direct
    SubscriberCallbackInfo * replay_callback_info =
        new SubscriberCallbackInfo ();
    replay_callback_info->callback = &replay_message_callback;
    replay_callback_info->data = this;
    this->spread.subscribe ("", this->replication_group, REPLAY_MESSAGE_TYPE,
                            replay_callback_info);
    this->spread.subscribe ("", this->spread.get_private_group (),
                            REPLAY_MESSAGE_TYPE, replay_callback_info);
}

void ReplicationRecorder::record ()
{
    this->spread.run (0);
}

bool ReplicationRecorder::handle_orig_message
(const std::string & /* sender */,
 const std::vector<std::string> & /* groups */,
 const int /* message_type */, const char * message, const int message_len)
{
    T_DEBUG ( "handle_orig_message:");

    // TODO: don't recreate these every time...
    shared_ptr<TMemoryBuffer> mbuf (new TMemoryBuffer ());
    TBinaryProtocol prot (mbuf);

    Message * m = new Message ();

    // deseralize the message
    mbuf->resetBuffer ((uint8_t*)message, message_len);
    m->read (&prot);

    T_DEBUG ( "handle_orig_message:    m.uuid=%s",  m->uuid.c_str());

    RecordedMessage * rm = new RecordedMessage ();
    rm->message = m;
    rm->buf = (char *)malloc (sizeof (char) * message_len);
    memcpy (rm->buf, message, message_len);
    rm->len = message_len;
    this->messages.push_back (rm);

    return true;
}

bool ReplicationRecorder::handle_replay_message
(const std::string & sender,
 const std::vector<std::string> & /* groups */,
 const int /* message_type */, const char * message, const int /* message_len */)
{
    string uuid = message;
    T_DEBUG (logger, "handle_replay_message: uuid=%s", uuid.c_str());

    RecordedMessage * next = NULL;
    vector<RecordedMessage *>::iterator i;
    for (i = this->messages.begin (); i < this->messages.end (); i++)
    {
        if ((*i)->message->uuid == uuid)
        {
            i++;
            if (i < this->messages.end ())
            {
                next = *i;
                break;
            }
        }
    }

    if (next)
    {
        T_DEBUG ("handle_replay_message: found uuid, returning next.uuid=%s",
                 next->message->uuid.c_str());
        // send it
        this->spread.queue (RELIABLE_MESS | SELF_DISCARD, sender,
                            REPLAY_MESSAGE_TYPE, next->buf, next->len);
    }
    else
    {
        T_DEBUG ("handle_replay_message: no next, returning NULL");
        // send none response
        string none = "none";
        this->spread.queue (RELIABLE_MESS | SELF_DISCARD, sender,
                            REPLAY_MESSAGE_TYPE, none.c_str (),
                            none.length ());
    }

    return true;
}

#endif /* HAVE_LIBSPREAD && HAVE_LIBUUID */
