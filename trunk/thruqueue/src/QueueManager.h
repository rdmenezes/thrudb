/**
 * Copyright (c) 2007- T Jake Luciani
 * Distributed under the New BSD Software License
 *
 * See accompanying file LICENSE or visit the Thrudb site at:
 * http://thrudb.googlecode.com
 *
 **/

#ifndef __QUEUE_MANAGER__
#define __QUEUE_MANAGER__

#include <string>
#include <boost/shared_ptr.hpp>
#include <concurrency/Thread.h>
#include <concurrency/Mutex.h>
#include <concurrency/Monitor.h>
#include <concurrency/PosixThreadFactory.h>

#include "Queue.h"

/**
 *Holds named array of active queues.
 *
 **/
class _QueueManager : public apache::thrift::concurrency::Runnable
{
 public:
    void   startup();

    void   createQueue ( const std::string &id, bool unique = false );
    void   destroyQueue( const std::string &id );
    boost::shared_ptr<Queue> getQueue( const std::string &id );

    static _QueueManager* instance();

 private:
    _QueueManager() : started(false){};
    void   run();
    apache::thrift::concurrency::Mutex mutex;

    std::map<std::string, boost::shared_ptr<Queue> > queue_cache;

    bool   started;
    static _QueueManager* pInstance;
    static apache::thrift::concurrency::Mutex _mutex;
};

//Singleton shortcut
#define QueueManager _QueueManager::instance()

#endif
