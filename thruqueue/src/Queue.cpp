/**
 * Copyright (c) 2007- T Jake Luciani
 * Distributed under the New BSD Software License
 *
 * See accompanying file LICENSE or visit the Thrudb site at:
 * http://thrudb.googlecode.com
 *
 **/
#ifdef HAVE_CONFIG_H
#include "thruqueue_config.h"
#endif
/* hack to work around thrift and log4cxx installing config.h's */
#undef HAVE_CONFIG_H

#include "Queue.h"
#include "ConfigFile.h"
#include "utils.h"
#include "QueueLog.h"
#include "ThruLogging.h"

#include <stdexcept>

#include <concurrency/ThreadManager.h>
#include <concurrency/Mutex.h>
#include <concurrency/PosixThreadFactory.h>
#include <protocol/TBinaryProtocol.h>
#include <transport/TTransportUtils.h>

using namespace std;
using namespace boost;
using namespace apache::thrift;
using namespace apache::thrift::concurrency;
using namespace apache::thrift::transport;
using namespace apache::thrift::protocol;
using namespace thruqueue;



/**
 *
 * A thruqueue is a series of messages logged to a thrift file transport.
 * This trasport is processed sequentially and has a
 * live compaction routine which runs to shrink the log.
 * The compaction routine can be configured to run based on the size of the processed log.
 *
 * Every log message contains the some standard info about the queue state
 * primarily to help speed up the compaction process.
 *
 * The queue messages are read and locked while a worker is performing a task on the message.
 * And when the task is completed sucessfully the worker must explicitly delete the message,
 * otherwise it will be placed back on the queue.
 *
 * This means we need to track the outstanding read messages that have not been deleted and know what chunks they live in.
 * so we can propagate them to the next logfile during compaction. to do this we keep another log of reads and deletes.
 *
 * Also, on startup we need to jump to the last known read position and, if compaction is required,
 * we write all the unread messages to the new log and build a list of the read but non-deleted
 * messages and their locations in the log.
 *
 * last   read  chunk.
 * last   delete chunk.
 * last   send chunk.
 * last   compaction timestamp.
 * we know the current chunk of the log reader.
 * (encode the expire timestamp into the message_id!)
 **/



/**
 *This is used to gather the latest queue stats from the tail of
 *the log. the tricky part here is starting back enough to capture the final message.
 *(i think) there is a chance this could fail if the final message is huge!
 *
 **/
class PruneCollector : virtual public QueueLogIf
{
public:
    PruneCollector() :
        queue_length(-1),last_read_chunk(-1),last_timestamp(-1){};

    void log_send(const QueueInputMessage &m){
        queue_length   = m.queue_length;
        last_timestamp = m.timestamp;
    }

    void log_read(const QueueOutputMessage &m){
        last_read_chunk = m.last_read_chunk;
        last_read_id    = m.message_id;
    }

    void log_delete(const QueueOutputMessage &m){

        if(m.timestamp > last_timestamp){
            queue_length   = m.queue_length;
            last_timestamp = m.timestamp;
        }

    }

    int32_t           queue_length;
    int32_t           last_read_chunk;
    std::string       last_read_id;
    int32_t           last_timestamp;
};




/**
 * This class handles the copying of good messages to a new log.
 * it does this by calling a fake client to a memory buffer then
 * saving the string to the log.
 *
 * this handles is called from the log processor for queue inputs
 * and queue outputs.
 **/
class PruneHandler : virtual public QueueLogIf
{
public:
    PruneHandler(shared_ptr<TFileTransport> _send_log, shared_ptr<TFileTransport> _read_log,
                 apache::thrift::concurrency::Mutex &mutex) :
        send_log(_send_log), read_log(_read_log), queue_length(0)
    {
        transport   = shared_ptr<TMemoryBuffer>(new TMemoryBuffer());
        shared_ptr<TProtocol>      p(new TBinaryProtocol(transport));
        faux_client = shared_ptr<QueueLogClient>(new QueueLogClient(p));
    };


    void log_read(const QueueOutputMessage &m )
    {
        read_cache[m.message_id] = m;
    }

    void log_delete(const QueueOutputMessage &m )
    {

        if(read_cache.count(m.message_id)){
            read_cache.erase(m.message_id);
            delete_cache[m.message_id]++;
        }
    }

    void log_send(const QueueInputMessage &m )
    {

        //skip any messages that have been deleted
        if(delete_cache.count(m.message_id))
            return;


        //re-add messages that have expired
        //otherwise keep them in the read log
        if(read_cache.count(m.message_id)){

            QueueOutputMessage m_read = read_cache[m.message_id];

            int32_t now = time(NULL);
            if(now < m_read.timestamp + m_read.lock_time){

                faux_client->send_log_read(m_read);
                string s = transport->getBufferAsString();

                transport->resetBuffer();

                //write it to log
                read_log->write( (uint8_t *)s.c_str(), (uint32_t) s.length() );
            }
        }



        faux_client->send_log_send(m);
        string s = transport->getBufferAsString();

        transport->resetBuffer();

        //write it to log
        send_log->write( (uint8_t *)s.c_str(), (uint32_t) s.length() );
    }



    shared_ptr<TFileTransport> send_log;
    shared_ptr<TFileTransport> read_log;
    shared_ptr<QueueLogClient> faux_client;
    shared_ptr<TMemoryBuffer>  transport;

    //tracks messages that were read/deleted
    map<string,QueueOutputMessage>  read_cache;
    map<string,int>                 delete_cache;

    unsigned int      queue_length;
};


/**
 *This interface tails the log and adds messages to the in-memory queue
 *
 **/
class QueueLogReader : virtual public QueueLogIf
{
public:
    QueueLogReader( Queue *_queue )
        : queue(_queue) {};

    void log_send(const QueueInputMessage &m ){
        queue->push_back(m);
    }

    void log_read(const QueueOutputMessage &m){

    }

    void log_delete(const QueueOutputMessage &m){

    }

    Queue *queue;
};


Queue::Queue(const string name)
    : queue_name(name), queue_length(0), is_pruning(false), msg_buffer_size(200)
{
    string doc_root    = ConfigManager->read<string>("DOC_ROOT");

    if( !directory_exists(doc_root) )
        throw std::runtime_error("DOC_ROOT is not valid (check config)");

    queue_input_log_file  = doc_root + "/"+queue_name+"_input.log";
    queue_output_log_file = doc_root + "/"+queue_name+"_output.log";


    //Create the faux log client
    transport = boost::shared_ptr<TMemoryBuffer>(new TMemoryBuffer());
    boost::shared_ptr<TProtocol>  p(new TBinaryProtocol(transport));
    queue_log_client             = shared_ptr<QueueLogClient>(new QueueLogClient(p));


    //Create input log
    queue_input_log  = shared_ptr<TFileTransport>(new TFileTransport(queue_input_log_file) );
    queue_input_log->setFlushMaxUs(100);


    //Create log processor to read from
    queue_input_log_reader       = shared_ptr<TFileTransport>(new TFileTransport(queue_input_log_file,true) );
    shared_ptr<TProtocolFactory>   pfactory(new TBinaryProtocolFactory());
    shared_ptr<QueueLogReader>     qlr(new QueueLogReader(this));
    shared_ptr<QueueLogProcessor>  proc    (new QueueLogProcessor(qlr));
    queue_input_log_processor    = shared_ptr<TFileProcessor>(new TFileProcessor(proc,pfactory,queue_input_log_reader));


    //Create output log
    queue_output_log  = shared_ptr<TFileTransport>(new TFileTransport(queue_output_log_file) );

    queue_output_log->setFlushMaxUs(100);
    queue_output_log->seekToEnd();

    //check if compaction is required on startup
    this->pruneLogFile();
}



void Queue::pruneLogFile()
{
    T_DEBUG("In pruneLogFile %s",queue_name.c_str());

    //another thread?
    if(is_pruning)
        return;

    //First we replay the tail end of the queue log and collect last know reader position
    shared_ptr<PruneCollector> pc(new PruneCollector());
    try{


        T_DEBUG("Finding location of last read");
        {
            Guard g(mutex);
            queue_input_log->flush();
            queue_output_log->flush();
        }

        {
            //init new log reader
            shared_ptr<TFileTransport>           in_log( new TFileTransport(queue_input_log_file,true) );
            shared_ptr<TFileTransport>           out_log( new TFileTransport(queue_output_log_file,true) );
            shared_ptr<QueueLogProcessor>        proc( new QueueLogProcessor(pc));
            shared_ptr<TProtocolFactory>         pfactory(new TBinaryProtocolFactory());

            //No need to do anything
            if(in_log->getNumChunks() < 1 && out_log->getNumChunks() < 1 && queue_length > 0 ) {
                T_DEBUG("Log too small %d %d", in_log->getNumChunks(), out_log->getNumChunks()  );
                return;
            }

            //just need the last few messages
            in_log->seekToChunk( in_log->getNumChunks()-1 );
            out_log->seekToChunk( out_log->getNumChunks()-1 );

            T_DEBUG("last chunk check: start");

            TFileProcessor input_fileProcessor(proc,pfactory,in_log);
            TFileProcessor output_fileProcessor(proc,pfactory,out_log);

            input_fileProcessor.process(0,false);
            output_fileProcessor.process(0,false);

            T_DEBUG("last chunk check: end");
        }


        T_DEBUG("queue_len %d",pc->queue_length);
        if(queue_length == 0 && pc->queue_length > 0){
            queue_length = pc->queue_length;
            T_DEBUG("queue_len %d",queue_length);
        }

        //FIXME: make this a config var
        if(pc->last_read_chunk < 10){
            T_INFO("reader low on queue, no compaction required: %d", pc->last_read_chunk);
            return;
        }

    }catch(TException e){

        T_ERROR("ex %s",e.what());
        return;
    }catch(...){

        perror("compaction failed big time");
    }


    T_DEBUG("Compaction begun");

    /////////////////////////////////////////////////
    //Notify other threads that we are about to prune
    //and start a new log
    string backup_input_log;
    string backup_output_log;

    try{
        Guard g(mutex);
        is_pruning = true;
        queue_input_log->flush();
        queue_output_log->flush();
        queue.clear();

        //////////////////////////////
        //backup the input old log
        char buf[512];
        sprintf(buf,".%d",(int)time(NULL));
        backup_input_log  = queue_input_log_file + buf;

        int rc = rename(queue_input_log_file.c_str(),backup_input_log.c_str());
        assert(rc == 0); //FIXME: better have worked

        //create new log and new reader
        queue_input_log   =    shared_ptr<TFileTransport>(new TFileTransport(queue_input_log_file) );
        queue_input_log->setFlushMaxUs(100);

        //create log processor to read new messages from
        queue_input_log_reader       = shared_ptr<TFileTransport>(new TFileTransport(queue_input_log_file,true) );
        shared_ptr<QueueLogReader>     qlr ( new QueueLogReader(this));
        shared_ptr<QueueLogProcessor>  proc( new QueueLogProcessor(qlr));
        shared_ptr<TProtocolFactory>   pfactory(new TBinaryProtocolFactory());
        queue_input_log_processor          = shared_ptr<TFileProcessor>(new TFileProcessor(proc,pfactory,queue_input_log_reader));

        /////////////////////////////////
        //backup the output log
        backup_output_log = queue_output_log_file + buf;
        rc = rename(queue_output_log_file.c_str(),backup_output_log.c_str());
        assert(rc == 0); //FIXME: better have worked

        //create new log and new reader
        queue_output_log   =    shared_ptr<TFileTransport>(new TFileTransport(queue_output_log_file) );
        queue_output_log->setFlushMaxUs(100);

    }catch(TException e){

        T_ERROR("ex2 %s",e.what());
        return;
    }catch(...){

        perror("compaction failed big time");
    }

    T_DEBUG("Populating new log()");

    //Replay old log to create a new pruned log


    //Log file to process
    shared_ptr<TFileTransport>           in_log_old(new TFileTransport(backup_input_log,true) );
    shared_ptr<TFileTransport>           out_log_old(new TFileTransport(backup_output_log,true) );

    //start at old reader position
    in_log_old->seekToChunk(pc->last_read_chunk);

    //Prune handler does the work
    shared_ptr<PruneHandler>             ph      (new PruneHandler(queue_input_log, queue_output_log, mutex));
    shared_ptr<QueueLogProcessor>        proc    (new QueueLogProcessor(ph));
    shared_ptr<TProtocolFactory>         pfactory(new TBinaryProtocolFactory());

    TFileProcessor output_fileProcessor(proc,pfactory,out_log_old);
    TFileProcessor input_fileProcessor(proc,pfactory,in_log_old);

    output_fileProcessor.process(0,false);
    input_fileProcessor.process(0,false);

    //Log file should be pruned. now lets populate our meta info
    {
        Guard g(mutex);
        is_pruning = false;
        T_DEBUG("Kept Unread Messages()");
        this->queue_length = ph->queue_length;
    }

    unlink(backup_input_log.c_str());
    unlink(backup_output_log.c_str());

    T_DEBUG("Exiting pruneLogFile()");
}

void Queue::push_back(const thruqueue::QueueInputMessage &m)
{
    queue.push_back(m);
}

void Queue::sendMessage(const string &mess)
{
    if(mess.empty())
        return;

    Guard g(mutex);

    QueueInputMessage m;
    m.message_id = generateUUID();
    m.message    = mess;

    //mark the chunk this message will live in our log
    m.chunk_pos = queue_input_log->getCurChunk();
    m.queue_length = queue_length;

    this->sendMessage(m);
}


void Queue::sendMessage(const QueueInputMessage &m)
{

    //This is called via log handler
    //FIXME: should be private with friend

    queue_log_client->send_log_send(m);

    string s = transport->getBufferAsString();
    transport->resetBuffer();

    //write it to log
    queue_input_log->write( (uint8_t *)s.c_str(), (uint32_t) s.length() );
    queue_length++;
}

QueueMessage Queue::readMessage(const int32_t &lock_time)
{
    if(lock_time < 0 || lock_time > 60*60*4){
        ThruqueueException e;
        e.code = INVALID_LOCK;
        e.what = "A message lock cannot be greater than 4 hours";
    }


    Guard g(mutex);

    if(queue_length == 0){

        ThruqueueException e;
        e.code = EMPTY_QUEUE;
        e.what = "The queue is empty";

        throw e;
    }

    if(queue.size() == 0)
        this->bufferMessagesFromLog();


    QueueMessage result;

    if(queue.size() > 0){

        QueueInputMessage m =  queue[queue.size()-1];
        queue.pop_back();

        QueueOutputMessage m_log;
        m_log.message_id      = m.message_id;
        m_log.lock_time       = lock_time;
        m_log.last_read_chunk = m.chunk_pos;

        queue_log_client->send_log_read(m_log);

        string s = transport->getBufferAsString();
        transport->resetBuffer();

        //write it to log
        queue_output_log->write( (uint8_t *)s.c_str(), (uint32_t) s.length() );

        result.message_id = m.message_id;
        result.message    = m.message;


    } else {

        T_INFO("queue length is > 0 but queue appears empty");

        ThruqueueException e;
        e.code = EMPTY_QUEUE;
        e.what = "The queue is empty";

        throw e;
    }

    queue_length--;

    return result;
}

void Queue::deleteMessage(const std::string &message_id)
{
        QueueOutputMessage m_log;
        m_log.message_id      = message_id;
        m_log.timestamp       = time(NULL);

        queue_log_client->send_log_delete(m_log);

        string s = transport->getBufferAsString();
        transport->resetBuffer();

        //write it to log
        queue_output_log->write( (uint8_t *)s.c_str(), (uint32_t) s.length() );
}


unsigned int Queue::length()
{
    Guard g(mutex);
    return queue_length;
}

void Queue::bufferMessagesFromLog()
{

    T_DEBUG("buffering new messages from log");

    if(queue.size() > 0)
        return;

    if(queue_length == 0)
        return;

    //Buffer a set of new items into the queue from the log
    if( queue_length <= msg_buffer_size ){

        //There's enough to fill the buffer, load it all
        queue_input_log_processor->process(queue_length,false);

        //Hmm, nada try flushing the log and reading again
        if(queue.size() == 0)
            queue_input_log->flush();

        queue_input_log_processor->process(queue_length,false);

        T_DEBUG("%d %d",queue.size(), queue_length);

    } else {

        while(queue.size() < msg_buffer_size){
            queue_input_log_processor->process(1,false);
        }
    }

}

void Queue::clear()
{
    Guard g(mutex);

    if(is_pruning){
        ThruqueueException e;
        e.what = BUSY_QUEUE;
        e.what = "Clear failed because queue log is being pruned, try again in a few";
        throw e;
    }

    queue_input_log->flush();
    queue_input_log        =    shared_ptr<TFileTransport>();

    //delete the current log
    unlink(queue_input_log_file.c_str());

    //create a new log file
    queue_input_log        =    shared_ptr<TFileTransport>(new TFileTransport(queue_input_log_file) );
    queue_input_log->setFlushMaxUs(100);

    //create new log processor
    queue_input_log_reader             = shared_ptr<TFileTransport>(new TFileTransport(queue_input_log_file,true) );
    shared_ptr<TProtocolFactory>   pfactory(new TBinaryProtocolFactory());
    shared_ptr<QueueLogReader>     qlr(new QueueLogReader(this));
    shared_ptr<QueueLogProcessor>  proc    (new QueueLogProcessor(qlr));
    queue_input_log_processor          = shared_ptr<TFileProcessor>(new TFileProcessor(proc,pfactory,queue_input_log_reader));



    //////output
    queue_output_log->flush();
    queue_output_log        =    shared_ptr<TFileTransport>();

    //delete the current log
    unlink(queue_output_log_file.c_str());

    //create a new log file
    queue_output_log        =    shared_ptr<TFileTransport>(new TFileTransport(queue_output_log_file) );
    queue_output_log->setFlushMaxUs(100);

    //Clear eveything
    queue_length = 0;
    queue.clear();
}
