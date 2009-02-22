namespace cpp  thruqueue
namespace java thruqueue
namespace php  Thruqueue
namespace perl Thruqueue


enum   ThruqueueExceptionCodes
{
        NONE          = 0,
        INVALID_QUEUE = 1,
        BUSY_QUEUE    = 2,
        EMPTY_QUEUE   = 3,
        INVALID_LOCK  = 4
}

exception ThruqueueException
{
        1:ThruqueueExceptionCodes code = NONE,
        2:string what
}


struct QueueMessage
{
        1:string message_id,
        2:string message
}

service Thruqueue
{
        void            ping()                                               throws(ThruqueueException e),
        list<string>    listAllQueues()                                      throws(ThruqueueException e),
        void            createQueue(1:string queue_name)                     throws(ThruqueueException e),
        void            deleteQueue(1:string queue_name)                     throws(ThruqueueException e),
        void            sendMessage(1:string queue_name, 2:string msg)          throws(ThruqueueException e),
        QueueMessage    readMessage(1:string queue_name, 2:i32 lock_time = 120) throws(ThruqueueException e),
        void            deleteMessage(1:string queue_name, 2:string message_id) throws(ThruqueueException e),
        void            clearQueue (1:string queue_name)                        throws(ThruqueueException e),
        i32             queueLength(1:string queue_name)                        throws(ThruqueueException e),

        string          admin(1:string op, 2:string data)                       throws(ThruqueueException e)
}

struct QueueOutputMessage
{
        1:string     message_id,
        2:i32        timestamp        = -1,
        3:i32        lock_time        = -1,
        8:i32        last_read_chunk  = -1,
        9:i32        queue_length     = -1,
        11:i32       chunk_pos        = -1
}

struct QueueInputMessage
{
        1:string     message_id,
        2:string     message,
        3:i32        timestamp        = -1,
        4:i32        queue_length     = -1,
        5:i32        chunk_pos        = -1
}

service QueueLog
{       void log_send(1:QueueInputMessage m);
        void log_read(1:QueueOutputMessage m);
        void log_delete(1:QueueOutputMessage m);
}
