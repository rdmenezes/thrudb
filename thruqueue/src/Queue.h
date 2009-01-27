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
    Queue(const std::string name, bool unique = false);

    void         enqueue(const thruqueue::QueueMessage &mess);
    void         enqueue(const std::string &mess, bool priority = false);
    std::string  dequeue();
    std::string  peek();

    unsigned int length();
    void         flush();

 private:
    void pruneLogFile();
    void bufferMessagesFromLog();


    facebook::thrift::concurrency::Mutex mutex;

    std::set<std::string>   unique_keys;

    std::deque<thruqueue::QueueMessage> queue;
    std::deque<std::string>             pruning_queue;
    std::deque<std::string>             priority_queue;

    std::string             queue_name;
    bool                    is_unique;
    std::string             queue_log_file;
    unsigned int            queue_length;


    bool is_pruning;
    boost::shared_ptr<facebook::thrift::transport::TFileTransport> queue_log;
    boost::shared_ptr<facebook::thrift::transport::TFileTransport> queue_log_reader;

    boost::shared_ptr<facebook::thrift::transport::TMemoryBuffer>  transport;
    boost::shared_ptr<thruqueue::QueueLogClient>                   queue_log_client;

    boost::shared_ptr<facebook::thrift::transport::TFileProcessor> queue_log_processor;


    unsigned int                msg_buffer_size;
};

#endif
