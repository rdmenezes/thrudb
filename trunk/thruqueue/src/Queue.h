/**
 * Copyright (c) 2007- T Jake Luciani
 * Distributed under the New BSD Software License
 *
 * See accompanying file LICENSE or visit the Thrudb site at:
 * http://thrudb.googlecode.com
 *
 **/


#ifndef __QUEUE__H__
#define __QUEUE__H__

#include <set>
#include <string>
#include <deque>

#include <concurrency/Thread.h>
#include <concurrency/Mutex.h>
#include <concurrency/Monitor.h>
#include <transport/TFileTransport.h>
#include <transport/TTransportUtils.h>

#include "Thruqueue.h"
#include "Thruqueue_types.h"
#include "Thruqueue_constants.h"

#include "QueueLog.h"


class Queue
{
 public:
    Queue(const std::string name);

    void                     sendMessage(const thruqueue::QueueInputMessage &mess);
    void                     sendMessage(const std::string &mess);
    thruqueue::QueueMessage  readMessage(const int32_t &lock_time);
    void                     deleteMessage(const std::string &message_id);

    unsigned int length();
    void         clear();
    void         push_back(const thruqueue::QueueInputMessage &m);

    void pruneLogFile();
 private:

    void bufferMessagesFromLog();


    apache::thrift::concurrency::Mutex mutex;

    //message_id, expire_time
    std::map<std::string, thruqueue::QueueOutputMessage> locked_messages;

    std::deque<thruqueue::QueueInputMessage> queue;



    std::string             queue_name;
    std::string             queue_input_log_file;
    std::string             queue_output_log_file;
    unsigned int            queue_length;


    bool is_pruning;

    //A input log holds all send messages
    boost::shared_ptr<apache::thrift::transport::TFileTransport> queue_input_log;
    boost::shared_ptr<apache::thrift::transport::TFileTransport> queue_input_log_reader;

    //A output log holds all read and delete messages
    boost::shared_ptr<apache::thrift::transport::TFileTransport> queue_output_log;


    boost::shared_ptr<apache::thrift::transport::TMemoryBuffer>  transport;
    boost::shared_ptr<thruqueue::QueueLogClient>                   queue_log_client;

    boost::shared_ptr<apache::thrift::transport::TFileProcessor> queue_input_log_processor;

    unsigned int                msg_buffer_size;
};

#endif
