#ifdef HAVE_CONFIG_H
#include "thrudoc_config.h"
#endif
/* hack to work around thrift and log4cxx installing config.h's */
#undef HAVE_CONFIG_H

#if HAVE_LIBSPREAD && HAVE_LIBUUID

#include "ReplicationBackend.h"

#include <errno.h>
#include <poll.h>
#include <protocol/TBinaryProtocol.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <transport/TTransportUtils.h>

#define MAX_BUCKET_SIZE 64
#define MAX_KEY_SIZE 64
#define MAX_VALUE_SIZE 2048

#define ORIG_MESSAGE_TYPE 1
#define REPLAY_MESSAGE_TYPE 101

using namespace boost;
using namespace apache::thrift::concurrency;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace std;
using namespace thrudoc;

string SP_error_to_string (int error);

class ReplicationWait
{
    public:
        // uuid is just for logging/debugging purposes
        ReplicationWait (string uuid, uint32_t max_wait)
        {

            T_DEBUG("ReplicationWait: uuid=%s, max_wait=%d",
                     uuid.c_str (), max_wait);


            this->uuid = uuid;
            this->max_wait = max_wait;

            pthread_mutex_init (&this->mutex, NULL);
            pthread_cond_init (&this->condition, NULL);
            // take the mutex now, so that release can't happen before
            // we're waiting on it
            pthread_mutex_lock (&this->mutex);
        }

        ~ReplicationWait ()
        {
            T_DEBUG ("~ReplicationWait: uuid=%s" + this->uuid.c_str());
            pthread_cond_destroy (&this->condition);
            pthread_mutex_destroy (&this->mutex);
        }

        void wait ()
        {
            T_DEBUG ("wait: uuid=%s", this->uuid.c_str());
#if defined (HAVE_CLOCK_GETTIME)
            int err;
            struct timespec abstime;
            err = clock_gettime(CLOCK_REALTIME, &abstime);
            if (!err)
            {
                abstime.tv_sec += this->max_wait;
                // cond_timedwait will unlock mutex so release can happen
                err = pthread_cond_timedwait (&this->condition, &this->mutex,
                                              &abstime);
                // we need to free the mutex back up, cond_timedwait will lock
                // it before it comes out
                pthread_mutex_unlock (&this->mutex);
                if (err == ETIMEDOUT)
                {
                    this->exception.what = "replication timeout exceeded";
                    T_INFO("wait: %s", this->exception.what);
                    return;
                }
            }
            else
                pthread_cond_wait (&this->condition, &this->mutex);
#else
            pthread_cond_wait (&this->condition, &this->mutex);
            pthread_mutex_unlock (&this->mutex);
#endif
        }

        void release (string ret, ThrudocException exception)
        {
            // we'll wait on the mutex to free up so that we don't send a
            // release before someone else is waiting on it
            pthread_mutex_lock (&this->mutex);

            this->ret = ret;
            this->exception = exception;
            pthread_cond_signal (&this->condition);
            // we've sent the signnal unlock our mutex
            pthread_mutex_unlock (&this->mutex);
        }

        string get_ret ()
        {
            return this->ret;
        }

        ThrudocException get_exception ()
        {
            return this->exception;
        }

    private:


        string uuid;
        uint32_t max_wait;
        pthread_mutex_t mutex;
        pthread_cond_t condition;
        ThrudocException exception;
        string ret;

};

// private

ReplicationBackend::ReplicationBackend (shared_ptr<ThrudocBackend> backend,
                                        const string & replication_name,
                                        const string & replication_private_name,
                                        const string & replication_group,
                                        const string & replication_status_file,
                                        const int replication_status_flush_frequency) :
    spread (replication_name, replication_private_name)
{

    T_DEBUG( "ReplicationBackend: replication_name=%s, replication_private_name=%s, replication_group=%s, replication_status_file=%s, replication_status_flush_frequency=%d",
             replication_name.c_str (), replication_private_name.c_str (),
             replication_group.c_str (), replication_status_file.c_str (),
             replication_status_flush_frequency);


    this->set_backend (backend);
    this->replication_group = replication_group;
    this->replication_status_file = replication_status_file;
    this->replication_status_flush_frequency = replication_status_flush_frequency;

    // subscribe to "live" messages
    SubscriberCallbackInfo * live_callback_info = new SubscriberCallbackInfo ();
    live_callback_info->callback = &orig_message_callback;
    live_callback_info->data = this;
    this->spread.subscribe ("", this->replication_group, ORIG_MESSAGE_TYPE,
                            live_callback_info);

    // subscribe to "replay" messages
    SubscriberCallbackInfo * replay_callback_info =
        new SubscriberCallbackInfo ();
    replay_callback_info->callback = &replay_message_callback;
    replay_callback_info->data = this;
    this->spread.subscribe ("", this->spread.get_private_group (),
                            REPLAY_MESSAGE_TYPE, replay_callback_info);

    int fd;
    fd = ::open (this->replication_status_file.c_str (), 0x0,
                 S_IRUSR | S_IWUSR| S_IRGRP | S_IROTH);
    listener_live = true; // we're live unless we load a last_uuid in a sec
    if (fd)
    {
        T_DEBUG (logger, "ReplicationBackend: opened status file=%s",
                 this->replication_status_file.c_str());
        char buf[64] = "";
        ::read (fd, buf, 64);
        this->last_uuid = buf;
        ::close (fd);
        if (!this->last_uuid.empty ())
        {
            listener_live = false;
            request_next (this->last_uuid);
            T_DEBUG ( "ReplicationBackend: found last_uuid=%s",
                      this->last_uuid.c_str());
        }
    }

    // create the listener thread
    listener_thread_go = true;
    if (pthread_create(&listener_thread, NULL, start_listener_thread,
                       (void *)this) != 0)
    {
        char error[] = "ReplicationBackend: start_listener_thread failed\n";
        T_ERROR (error);
        ThrudocException e;
        e.what = error;
        throw e;
    }
}

ReplicationBackend::~ReplicationBackend ()
{
    T_DEBUG ( "~ReplicationBackend");
    // we're no longer live, don't accept connections
    this->listener_live = false;
    // tell the listener to exit
    this->listener_thread_go = false;
    // wait on the listner to exit
    if (listener_thread != 0)
        pthread_join(listener_thread, NULL);

    // rest of this needs to happen after we've joined the reader thread
    // and stopped taking new requests

    // it's string and shared_ptr so removing them from the map should make
    // them go away
    pending_waits.clear ();

    // need to do this after we stop the listener thread else we may never
    // empty it
    while (!pending_messages.empty ())
    {
        delete pending_messages.front ();
        pending_messages.pop ();
    }
}

// TODO: there are potential issues here if the local host successfully applies
// an operation that no one else can. i can't currently think of a way that can
// happen so i'm not too worried about it. but it is (in theory) a possiblity.

void ReplicationBackend::put (const string & bucket, const string & key,
                                    const string & value)
{
    map<string, string> params;
    params["bucket"] = bucket;
    params["key"] = key;
    params["value"] = value;
    this->send_orig_message_and_wait ("put", params);
}

void ReplicationBackend::remove (const string & bucket,
                                       const string & key )
{
    map<string, string> params;
    params["bucket"] = bucket;
    params["key"] = key;
    this->send_orig_message_and_wait ("remove", params);
}

string ReplicationBackend::admin (const string & op, const string & data)
{
    if (op == "replay_from")
    {
        this->listener_live = false;
        request_next (data);
        return "done";
    }
    map<string, string> params;
    params["op"] = op;
    params["data"] = data;
    return this->send_orig_message_and_wait ("admin", params);
}

void ReplicationBackend::validate (const std::string & bucket,
                                         const std::string * key,
                                         const std::string * value)
{
    if (!this->listener_live)
    {
        ThrudocException e;
        e.what = "not up to date, try again later";
        throw e;
    }
    this->get_backend ()->validate (bucket, key, value);
    if (bucket.length () > MAX_BUCKET_SIZE)
    {
        ThrudocException e;
        e.what = "bucket too large";
        throw e;
    }
    if (key != NULL && key->length () > MAX_KEY_SIZE)
    {
        ThrudocException e;
        e.what = "key too large";
        throw e;
    }
    if (value != NULL && value->length () > MAX_VALUE_SIZE)
    {
        ThrudocException e;
        e.what = "value too large";
        throw e;
    }
    // so long as the bucket, key, and value have valid sizes our msg len
    // should be ok
}

string ReplicationBackend::generate_uuid ()
{
    uuid_t uuid;
    uuid_generate(uuid);
    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);
    return string (uuid_str);
}

string ReplicationBackend::send_orig_message_and_wait
(string method, map<string, string> params)
{
    // TODO: don't recreate these every time...
    shared_ptr<TMemoryBuffer> mbuf (new TMemoryBuffer ());
    TBinaryProtocol prot (mbuf);

    // create the message;
    Message * m = new Message ();
    m->uuid = generate_uuid ();
    m->sender = this->spread.get_private_group ();
    m->method = method;
    m->params = params;

    // serialize it
    mbuf->resetBuffer ();
    m->write (&prot);
    string message = mbuf->getBufferAsString ();

    T_DEBUG ("send_orig_message_and_wait: begin uuid=%s",m->uuid);

    shared_ptr<ReplicationWait> wait (new ReplicationWait (m->uuid, 2));
    // install wait
    {
        RWGuard g (this->pending_waits_mutex, true);
        pending_waits[m->uuid] = wait;
    }
    // queue up multi-cast message
    this->spread.send (SAFE_MESS, this->replication_group, ORIG_MESSAGE_TYPE,
                       message.c_str (), message.length ());
    // wait here until we have the result
    wait->wait ();
    // uninstall wait
    {
        RWGuard g (this->pending_waits_mutex, true);
        pending_waits.erase (m->uuid);
    }
    T_DEBUG ( "send_orig_message_and_wait: done uuid=%s", m->uuid);

    // clean up
    delete m;

    // throw exception if we have one
    ThrudocException e = wait->get_exception ();
    if (!e.what.empty ())
    {
        throw wait->get_exception ();
    }
    // otherwise return the result
    return wait->get_ret ();
}

bool ReplicationBackend::handle_orig_message
(const std::string & /* sender */,
 const std::vector<std::string> & /* groups */,
 const int /* message_type */, const char * message, const int message_len)
{
    // TODO: don't recreate these every time...
    shared_ptr<TMemoryBuffer> mbuf (new TMemoryBuffer ());
    TBinaryProtocol prot (mbuf);

    Message * m = new Message ();

    // deseralize the message
    mbuf->resetBuffer ((uint8_t*)message, message_len);
    m->read (&prot);

    if (this->listener_live)
    {

        T_DEBUG( "handle_orig_message: pending_messages.size=%d",
                     (int)pending_messages.size ());

        // drain anything in the queue and then do this one.
        while (!this->pending_messages.empty ())
        {
            Message * drain = this->pending_messages.front ();
            this->pending_messages.pop ();
            T_DEBUG( "handle_orig_message: drain.uuid=%s",
                     drain->uuid.c_str());
            do_message (drain);
            delete drain;
        }
        T_DEBUG ("handle_orig_message: message.uuid=%s", m->uuid.c_str());
        do_message (m);
        delete m;
    }
    else
    {
        T_DEBUG ("handle_orig_message: push.uuid=%s", m->uuid.c_str());
        // queue it
        this->pending_messages.push (m);
    }
    // we always continue listening
    return true;
}

// TODO: handle when we don't hear from our replay host for a while...
bool ReplicationBackend::handle_replay_message
(const std::string & sender,
 const std::vector<std::string> & /* groups */,
 const int /* message_type */, const char * message, const int message_len)
{
    T_DEBUG ("handle_replay_message:");

    // first one to answer becomes the person we'll ask in the future
    if (this->current_replay_name.empty ())
    {
        this->current_replay_name = sender;
    }
    else if (this->current_replay_name != sender)
    {
        // this isn't from our offical replay host, skip it
        return true;
    }

    // TODO: don't recreate these every time...
    shared_ptr<TMemoryBuffer> mbuf (new TMemoryBuffer ());
    TBinaryProtocol prot (mbuf);

    if (strncmp ("none", message, 4) != 0)
    {
        Message * m = new Message ();

        // deseralize the message
        mbuf->resetBuffer ((uint8_t*)message, message_len);
        m->read (&prot);

        // it's a message
        Message * first_queued = pending_messages.front ();
        if (first_queued == NULL || first_queued->uuid != m->uuid)
        {
            // we haven't caught up yet
            request_next (m->uuid);
            T_DEBUG ( "handle_replay_message: catchup.uuid=%s",
                      m->uuid.c_str());
            do_message (m);
        }
        else
        {
            T_DEBUG ("handle_replay_message: caughtup.uuid=%s",
                     m->uuid.c_str());
            // we've caught back up
            this->listener_live = true;
        }

        delete m;
    }
    else
    {
        T_DEBUG ("handle_replay_message: caughtup.uuid=none");
        // we're out of stuff to replay, go back to live, hopefully nothing
        // actually happened since the last message we recorded
        this->listener_live = true;
        // unset our current_replay_name, so we'll get one again next time
        this->current_replay_name = "";
    }

    return true;
}

void * ReplicationBackend::start_listener_thread (void * ptr)
{
    T_DEBUG ( "start_listener_thread: ");
    (((ReplicationBackend*)ptr)->listener_thread_run ());
    return NULL;
}

void ReplicationBackend::listener_thread_run ()
{
    time_t last_flush = time (0);

    // TODO: this will never exit
    while (1)
    {
        // TODO: this is way too often
        try
        {
            this->spread.run (10);
        }
        catch (SpreadException & e)
        {
            T_ERROR ("listener_thread_run: exception e.what=%s",e.message.c_str());
            // stop the replication thread
            this->listener_thread_go = false;
        }
        catch (ThrudocException & e)
        {
            T_ERROR ("listener_thread_run: exception e.what=%s",e.what.c_str());
            // stop the replication thread
            this->listener_thread_go = false;
        }

        if ((last_flush + this->replication_status_flush_frequency) <
            time (0))
        {
            T_DEBUG ( "listener_thread_run: flushing last_uuid=%s",
                      this->last_uuid.c_str());
            int fd;
            fd = ::open (this->replication_status_file.c_str (),
                         O_RDWR | O_TRUNC | O_CREAT,
                         S_IRUSR | S_IWUSR| S_IRGRP | S_IROTH);
            ::write (fd, this->last_uuid.c_str (),
                     this->last_uuid.length ());
            fsync (fd);
            ::close (fd);
            last_flush = time (0);
        }
    }
}

void ReplicationBackend::do_message (Message * message)
{
    T_DEBUG ("do_message: message.method=%s", message->method.c_str());
    string ret;
    ThrudocException exception;
    try
    {
        if (message->method == "put")
        {
            string bucket = message->params["bucket"];
            string key = message->params["key"];
            string value = message->params["value"];


            this->get_backend ()->put (bucket, key, value);
        }
        else if (message->method == "remove")
        {
            string bucket = message->params["bucket"];
            string key = message->params["key"];


            this->get_backend ()->remove (bucket, key);
        }
        else if (message->method == "admin")
        {
            string op = message->params["op"];
            string data = message->params["data"];

            ret = this->get_backend ()->admin (op, data);
        }
        else
        {
            T_DEBUG ("replication unknown method=%s",
                                     message->method.c_str());
        }
    }
    catch (ThrudocException & e)
    {
        // TODO: we're catching and returning exceptions to the client here,
        // but we don't (yet) know when to stop replication when things are in
        // or will be in a broken state.
        exception = e;
    }
    catch (...)
    {
        exception.what = "unknown exception, that's not good...";
        T_ERROR ( exception.what);
    }

    // if we sent this message signal to the waiting thread that it's complete
    if (this->spread.get_private_group () == message->sender)
    {
        RWGuard g (this->pending_waits_mutex, false);
        std::map<std::string, boost::shared_ptr<ReplicationWait> >::iterator
            i = pending_waits.find (message->uuid);
        if (i != pending_waits.end ())
            (*i).second->release (ret, exception);
    }
    this->last_uuid = message->uuid;
    T_DEBUG ("setting last_uuid=%s", last_uuid);
}

void ReplicationBackend::request_next (string uuid)
{
    string who_to_ask = this->current_replay_name;
    if (who_to_ask.empty ())
    {
        // we don't have a current replay from, so ask them all, first one to
        // respond will be the the new replay host
        who_to_ask = this->replication_group;
    }
    T_DEBUG ("request_next: who_to_ask=%s", who_to_ask);

    // we don't want our own message back here...
    this->spread.send (RELIABLE_MESS | SELF_DISCARD, who_to_ask,
                       REPLAY_MESSAGE_TYPE, uuid.c_str (), uuid.length ());
}

#endif /* HAVE_LIBSPREAD && HAVE_LIBUUID */
