#ifdef HAVE_CONFIG_H
#include "thrudoc_config.h"
#endif
/* hack to work around thrift and log4cxx installing config.h's */
#undef HAVE_CONFIG_H

#if HAVE_LIBSPREAD && HAVE_LIBUUID

#include "ReplicationBackend.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <poll.h>

// should be max expected key + value + uuid size + ~4. truncation occur
// otherwise
#define MAX_BUCKET_SIZE 64
#define MAX_KEY_SIZE 64
#define MAX_VALUE_SIZE 2048
#define UUID_LEN 37
#define MESSAGE_OVERHEAD 4
#define REPLICATION_BACKEND_MAX_MESSAGE_SIZE MAX_BUCKET_SIZE + MAX_KEY_SIZE + MAX_VALUE_SIZE + UUID_LEN + MESSAGE_OVERHEAD

#define ORIG_MESSAGE_TYPE 1
#define REPLY_MESSAGE_TYPE 2
#define REPLAY_MESSAGE_TYPE 101

using namespace boost;
using namespace facebook::thrift::concurrency;
using namespace log4cxx;
using namespace std;
using namespace thrudoc;

string SP_error_to_string (int error);

class ReplicationWait 
{
    public:
        // uuid is just for logging/debugging purposes
        ReplicationWait (string uuid, uint32_t max_wait)
        {
            char buf[128];
            sprintf (buf, "ReplicationWait: uuid=%s, max_wait=%d", 
                     uuid.c_str (), max_wait);
            LOG4CXX_DEBUG (logger, buf);

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
            LOG4CXX_DEBUG (logger, "~ReplicationWait: uuid=" + 
                           this->uuid);
            pthread_cond_destroy (&this->condition);
            pthread_mutex_destroy (&this->mutex);
        }

        void wait ()
        {
            LOG4CXX_DEBUG (logger, "wait: uuid=" + this->uuid);
#if defined (HAVE_CLOCK_GETTIME)
            int err;
            struct timespec abstime;
            err = clock_gettime(CLOCK_REALTIME, &this->abstime);
            if (!err)
            {
                this->abstime.tv_sec += this->max_wait;
                // cond_timedwait will unlock mutex so release can happen
                err = pthread_cond_timedwait (&this->condition, &this->mutex, 
                                              &this->abstime);
                // we need to free the mutex back up, cond_timedwait will lock 
                // it before it comes out
                pthread_mutex_unlock (&this->mutex);
                if (err == ETIMEDOUT)
                {
                    this->exception.what = "replication timeout exceeded";
                    LOG4CXX_WARN (logger, "wait: " + this->exception.what);
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
            LOG4CXX_DEBUG (logger, "release: ret=" + ret + 
                           ", exception.what=" + exception.what +
                           ", uuid=" + this->uuid);
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
        static log4cxx::LoggerPtr logger;

        string uuid;
        uint32_t max_wait;
        pthread_mutex_t mutex;
        pthread_cond_t condition;
        ThrudocException exception;
        string ret;

};

class ReplicationMessage
{
    public:
        void receive (mailbox spread_mailbox)
        {
            max_groups = 5;
            buf_size = REPLICATION_BACKEND_MAX_MESSAGE_SIZE;

            buf_len = SP_receive (spread_mailbox, &service_type, sender,
                                  max_groups, &num_groups, groups, &type,
                                  &endian_mismatch, buf_size, buf);
            if (buf_len > 0)
            {
                // null terminate the message
                buf[buf_len] = '\0';
                LOG4CXX_DEBUG (logger, string ("receive: buf=") + buf);
            }
            else
            {
                ThrudocException e;
                switch (buf_len)
                {
                    case ILLEGAL_SESSION:
                        // ILLEGAL_SESSION - The mbox given to receive on was illegal.
                        e.what = "replication: spread error ILLEGAL_SESSION, stopping";
                        break;
                    case ILLEGAL_MESSAGE:
                        // ILLEGAL_MESSAGE - The message had an illegal structure, like a scatter not filled out correctly.
                        e.what = "replication: spread error ILLEGAL_MESSAGE, ignoring";
                        break;
                    case CONNECTION_CLOSED:
                        // CONNECTION_CLOSED - During  communication  to  receive  the  message  communication errors occured and the receive could not be completed.
                        e.what = "replication: spread error CONNECTION_CLOSED";
                        // TODO: try and reconnect
                        break;
                    case GROUPS_TOO_SHORT:
                        // GROUPS_TOO_SHORT - If the groups array is too short to hold  the  entire  list  of groups this message was sent to then this error is returned and the num_groups field will be set to the negative of the  number of groups needed.
                        e.what = "replication: spread error GROUPS_TOO_SHORT, stopping";
                        break;
                    case BUFFER_TOO_SHORT:
                        // BUFFER_TOO_SHORT - If  the  message body buffer mess is too short to hold the mes‐ sage being  received  then  this  error  is  returned  and  the endian_mismatch  field  is  set  to  the  negative value of the required buffer length.
                        e.what = "replication: spread error BUFFER_TOO_SHORT, stopping";
                        break;
                }
                throw e;
            }
        }

        int parse (const string & replication_private_group)
        {
            // TODO: need better/safer parsing than scanf... maybe thrift ser.
            command = 0;
            if (type == ORIG_MESSAGE_TYPE)
            {
                LOG4CXX_DEBUG (logger, "parse: orig");
                char uuid_buf[UUID_LEN];
                if (sscanf (buf, "%s %c %s %s %s", uuid_buf, &command, bucket, 
                            key, value))
                {
                    uuid = uuid_buf;
                    return type;
                }
            }
            else if (type == REPLAY_MESSAGE_TYPE &&
                     replication_private_group == groups[0])
            {
                LOG4CXX_DEBUG (logger, "parse: replay");
                const char * offset;
                if (strncmp (buf, ";;none", strlen (";;none")) == 0)
                {
                    // TODO: the recorder is out of data, we should do
                    // something more with that info here as well as blow the
                    // things up above here and say that we aren't able to 
                    // catch up... do take in to consideration catch-up on an
                    // inactive system
                    uuid = "";
                    return type;
                }

                offset = strrchr (buf, ';') + 1;
                char uuid_buf[UUID_LEN];
                if (sscanf (offset, "%s %c %s %s %s", uuid_buf, &command,
                            bucket, key, value))
                {
                    uuid = uuid_buf;
                    return type;
                }
            }
            return 0;
        }

        string get_uuid ()
        {
            return this->uuid;
        }

        const char * get_sender ()
        {
            return this->sender;
        }

        char get_command ()
        {
            return this->command;
        }

        const char * get_bucket ()
        {
            return this->bucket;
        }

        const char * get_key ()
        {
            return this->key;
        }

        const char * get_value ()
        {
            return this->value;
        }

        const char * get_op ()
        {
            return this->bucket;
        }

        const char * get_data ()
        {
            return this->key;
        }

    private:
        static log4cxx::LoggerPtr logger;

        // spread message stuff
        service service_type;
        char sender[MAX_GROUP_NAME];
        char max_groups;
        int num_groups;
        char groups[5][MAX_GROUP_NAME];
        int16_t type;
        int endian_mismatch;
        int buf_size;
        int buf_len;
        char buf[REPLICATION_BACKEND_MAX_MESSAGE_SIZE];

        // repli command stuff
        string uuid;
        char command;
        char bucket[MAX_BUCKET_SIZE];
        char key[MAX_KEY_SIZE];
        char value[MAX_VALUE_SIZE];
};

// private
LoggerPtr ReplicationWait::logger (Logger::getLogger ("ReplicationWait"));
LoggerPtr ReplicationMessage::logger (Logger::getLogger ("ReplicationMessage"));
LoggerPtr ReplicationBackend::logger (Logger::getLogger ("ReplicationBackend"));

ReplicationBackend::ReplicationBackend (shared_ptr<ThrudocBackend> backend,
                                        const string & replication_name,
                                        const string & replication_private_name,
                                        const string & replication_group,
                                        const string & replication_status_file,
                                        const int replication_status_flush_frequency)
{
    char buf[1024];
    sprintf (buf, "ReplicationBackend: replication_name=%s, replication_private_name=%s, replication_group=%s, replication_status_file=%s, replication_status_flush_frequency=%d",
             replication_name.c_str (), replication_private_name.c_str (),
             replication_group.c_str (), replication_status_file.c_str (),
             replication_status_flush_frequency);
    LOG4CXX_INFO (logger, buf);

    this->set_backend (backend);
    this->replication_name = replication_name;
    this->replication_private_name = replication_private_name;
    this->replication_group = replication_group;
    this->replication_status_file = replication_status_file;
    this->replication_status_flush_frequency = replication_status_flush_frequency;

    char private_group[MAX_GROUP_NAME];
    int ret = SP_connect (this->replication_name.c_str (),
                          this->replication_private_name.c_str (), 0, 1,
                          &this->spread_mailbox, private_group);
    if (ret < 0)
    {
        string error = SP_error_to_string (ret);
        LOG4CXX_ERROR (logger, error);
        ThrudocException e;
        e.what = error;
        throw e;
    }

    this->replication_private_group = string (private_group);
    LOG4CXX_INFO (logger, "ReplicationBackend: private_group=" +
                  this->replication_private_group);

    ret = SP_join (this->spread_mailbox, this->replication_group.c_str ());
    if (ret < 0)
    {
        string error = SP_error_to_string (ret);
        LOG4CXX_ERROR (logger, error);
        ThrudocException e;
        e.what = error;
        throw e;
    }

    int fd;
    fd = ::open (this->replication_status_file.c_str (), 0x0, 
                 S_IRUSR | S_IWUSR| S_IRGRP | S_IROTH);
    listener_live = true; // we're live unless we load a last_uuid in a sec
    if (fd)
    {
        LOG4CXX_DEBUG (logger, "ReplicationBackend: opened status file=" +
                       this->replication_status_file);
        char buf[64] = "";
        ::read (fd, buf, 64);
        this->last_uuid = buf;
        ::close (fd);
        if (!this->last_uuid.empty ())
        {
            listener_live = false;
            request_next (this->last_uuid);
            LOG4CXX_INFO (logger, "ReplicationBackend: found last_uuid=" +
                          this->last_uuid);
        }
    }

    // create the listener thread
    listener_thread_go = true;
    if (pthread_create(&listener_thread, NULL, start_listener_thread,
                       (void *)this) != 0)
    {
        char error[] = "ReplicationBackend: start_listener_thread failed\n";
        LOG4CXX_ERROR (logger, error);
        ThrudocException e;
        e.what = error;
        throw e;
    }
}

ReplicationBackend::~ReplicationBackend ()
{
    LOG4CXX_INFO (logger, "~ReplicationBackend");
    // we're no longer live, don't accept connections
    this->listener_live = false;
    // tell the listener to exit
    this->listener_thread_go = false;
    // wait on the listner to exit
    if (listener_thread != 0)
        pthread_join(listener_thread, NULL);

    // rest of this needs to happen after we've joined the reader thread
    // and stopped taking new requests

    // don't disconnect from spread until the reader thread has been joined
    // so that it can finish doing whatever it has going on.
    SP_disconnect (this->spread_mailbox);

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

// TODO: implement circuit breaker pattern around spread...

void ReplicationBackend::put (const string & bucket, const string & key,
                                    const string & value)
{
    char msg[REPLICATION_BACKEND_MAX_MESSAGE_SIZE];
    string uuid = generate_uuid ();
    snprintf (msg, REPLICATION_BACKEND_MAX_MESSAGE_SIZE, "%s p %s %s %s",
              uuid.c_str (), bucket.c_str (), key.c_str (), value.c_str ());

    this->send_and_wait_for_resp (msg, uuid);
}

void ReplicationBackend::remove (const string & bucket,
                                       const string & key )
{
    char msg[REPLICATION_BACKEND_MAX_MESSAGE_SIZE];
    string uuid = generate_uuid ();
    snprintf (msg, REPLICATION_BACKEND_MAX_MESSAGE_SIZE, "%s r %s %s",
              uuid.c_str (), bucket.c_str (), key.c_str ());

    this->send_and_wait_for_resp (msg, uuid);
}

string ReplicationBackend::admin (const string & op, const string & data)
{
    if (op == "replay_from")
    {
        this->listener_live = false;
        request_next (data);
        return "done";
    }
    char msg[REPLICATION_BACKEND_MAX_MESSAGE_SIZE];
    string uuid = generate_uuid ();
    snprintf (msg, REPLICATION_BACKEND_MAX_MESSAGE_SIZE, "%s a %s %s",
              uuid.c_str (), op.c_str (), data.c_str ());

    return this->send_and_wait_for_resp (msg, uuid);
}

string ReplicationBackend::send_and_wait_for_resp (const char * msg,
                                                         string uuid)
{
    LOG4CXX_DEBUG (logger, "wait_for_resp: begin uuid=" + uuid);
    string ret;
    shared_ptr<ReplicationWait> wait (new ReplicationWait (uuid, 
                                                                       2));
    // install wait
    {
        RWGuard g (this->pending_waits_mutex, true);
        pending_waits[uuid] = wait;
    }
    // send out multi-cast message
    SP_multicast (this->spread_mailbox, SAFE_MESS, 
                  this->replication_group.c_str (),
                  ORIG_MESSAGE_TYPE, strlen (msg), msg);
    // wait here until we have the result
    wait->wait ();
    // uninstall wait
    {
        RWGuard g (this->pending_waits_mutex, true);
        pending_waits.erase (uuid);
    }
    LOG4CXX_DEBUG (logger, "wait_for_resp: done uuid=" + uuid);
    // throw exception if we have one
    ThrudocException e = wait->get_exception ();
    if (!e.what.empty ())
    {
        throw wait->get_exception ();
    }
    // otherwise return the result
    return wait->get_ret ();
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

void * ReplicationBackend::start_listener_thread (void * ptr)
{
    LOG4CXX_INFO (logger, "start_listener_thread: ");
    (((ReplicationBackend*)ptr)->listener_thread_run ());
    return NULL;
}

void ReplicationBackend::listener_thread_run ()
{
    struct pollfd fds[] = { {this->spread_mailbox, POLLIN, 0} };
    time_t last_flush = time (0);

    while (this->listener_thread_go)
    {
        int ret = poll (fds, 1, 1000);
        if (ret > 0)
        {
            try
            {
                handle_message ();
            }
            catch (ThrudocException & e)
            {
                LOG4CXX_ERROR (logger, "listener_thread_run: exception e.what=" +
                               e.what);
                // stop the replication thread
                this->listener_thread_go = false;
            }
            if ((last_flush + this->replication_status_flush_frequency) < 
                time (0))
            {
                LOG4CXX_DEBUG (logger, "listener_thread_run: flushing last_uuid=" +
                               this->last_uuid);
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
        else if (ret < 0)
        {
            char buf[64];
            sprintf (buf, "listener_thread_run: poll ret=%d", ret);
            LOG4CXX_WARN (logger, buf);
        }
        /* else 0, just means timeout */
    }
}

void ReplicationBackend::handle_message ()
{
    ReplicationMessage * message = new ReplicationMessage ();
    message->receive (this->spread_mailbox);
    int type = message->parse (this->replication_private_group);
    if (type == ORIG_MESSAGE_TYPE)
    {
        if (this->listener_live)
        {
            // do any/all queued messages, normally when we're coming off of
            // a catch up and have been queueing up new message in the meantime
            if (logger->isDebugEnabled ())
            {
                char buf[64];
                sprintf (buf, "handle_message: pending_messages.size=%d", pending_messages.size ());
                LOG4CXX_DEBUG (logger, buf);
            }
            while (!pending_messages.empty ())
            {
                ReplicationMessage * drain = pending_messages.front ();
                pending_messages.pop ();
                LOG4CXX_DEBUG (logger, string ("handle_message: drain.uuid=") +
                               drain->get_uuid ());
                do_message (drain);
                delete drain;
            }
            LOG4CXX_DEBUG (logger, string ("handle_message: message.uuid=") +
                           message->get_uuid ());
            // then do this one
            do_message (message);
            delete message;
        }
        else
        {
            LOG4CXX_DEBUG (logger, string ("handle_message: push.uuid=") +
                           message->get_uuid ());
            pending_messages.push (message);
        }
    }
    else if (type == REPLAY_MESSAGE_TYPE)
    {
        if (!message->get_uuid ().empty ())
        {
            // it's a message
            ReplicationMessage * first_queued = pending_messages.front ();
            if (first_queued == NULL ||
                first_queued->get_uuid () != message->get_uuid ())
            {
                // we haven't caught up yet
                request_next (message->get_uuid ());
                LOG4CXX_DEBUG (logger, 
                               string ("handle_message: catchup.uuid=") +
                               message->get_uuid ());
                do_message (message);
            }
            else
            {
                LOG4CXX_DEBUG (logger, 
                               string ("handle_message: caughtup.uuid=") +
                               message->get_uuid ());
                // we've caught back up
                this->listener_live = true;
            }
        }
        else
        {
            LOG4CXX_DEBUG (logger, 
                           string ("handle_message: caughtup.uuid=none"));
            // we're out of stuff to replay, go back to live, hopefully nothing
            // actually happened since the last message we recorded
            this->listener_live = true;
        }
        delete message;
    }
    else
    {
        // it's not a message we're interested in
        delete message;
    }
}

void ReplicationBackend::do_message (ReplicationMessage * message)
{
    string ret;
    ThrudocException exception;
    try
    {
        switch (message->get_command ())
        {
            case 'p':
                LOG4CXX_DEBUG (logger, string ("replication: put: bucket=") +
                               message->get_bucket () + ", key=" +
                               message->get_key () + ", value=" +
                               message->get_value ());
                this->get_backend ()->put (message->get_bucket (),
                                           message->get_key (),
                                           message->get_value ());
                break;
            case 'r':
                LOG4CXX_DEBUG (logger, string ("replication remove: bucket=") +
                               message->get_bucket () + ", key=" +
                               message->get_key ());
                this->get_backend ()->remove (message->get_bucket (),
                                              message->get_key ());
                break;
            case 'a':
                LOG4CXX_DEBUG (logger, string ("replication admin: op=") +
                               message->get_op () + ", data=" +
                               message->get_data ());
                ret = this->get_backend ()->admin (message->get_op (),
                                                   message->get_data ());
                break;
            case 0:
            default:
                LOG4CXX_DEBUG (logger, string ("replication admin: unknown command"));
                break;
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
        LOG4CXX_WARN (logger, exception.what);
    }

    // if we sent this message signal to the waiting thread that it's complete
    if (this->replication_private_group == message->get_sender ())
    {
        RWGuard g (this->pending_waits_mutex, false);
        std::map<std::string, boost::shared_ptr<ReplicationWait> >::iterator
            i = pending_waits.find (message->get_uuid ());
        if (i != pending_waits.end ())
            (*i).second->release (ret, exception);
    }
    this->last_uuid = message->get_uuid ();
    LOG4CXX_DEBUG (logger, "setting last_uuid=" + last_uuid);
}

void ReplicationBackend::request_next (string uuid)
{
    // we don't want our own message back here...
    SP_multicast (this->spread_mailbox, RELIABLE_MESS | SELF_DISCARD,
                  this->replication_group.c_str (),
                  REPLAY_MESSAGE_TYPE, uuid.length (), uuid.c_str ());
}

#endif /* HAVE_LIBSPREAD && HAVE_LIBUUID */