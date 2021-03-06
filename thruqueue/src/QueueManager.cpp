
/**
 * Copyright (c) 2007- T Jake Luciani
 * Distributed under the New BSD Software License
 *
 * See accompanying file LICENSE or visit the Thrudb site at:
 * http://thrudb.googlecode.com
 *
 **/

#include "QueueManager.h"
#include "Thruqueue.h"
#include "ConfigFile.h"
#include "utils.h"
#include <stdexcept>

#include <concurrency/ThreadManager.h>
#include <concurrency/Mutex.h>
#include <concurrency/PosixThreadFactory.h>
#include <protocol/TBinaryProtocol.h>
#include <transport/TTransportUtils.h>

using namespace std;
using namespace boost;
using namespace apache::thrift;
using namespace thruqueue;
using namespace apache::thrift::concurrency;
using namespace apache::thrift::transport;
using namespace apache::thrift::protocol;


_QueueManager* _QueueManager::pInstance = 0;
Mutex          _QueueManager::_mutex    = Mutex();

struct null_deleter
{
    void operator()(void const *) const {}
};


/**
 * The queue manager keeps tabs on the queues and makes sure they are being cleaned
 *
 **/
_QueueManager *_QueueManager::instance()
{
    if(pInstance == 0){
        Guard guard(_mutex);

        if(pInstance == 0){
            pInstance = new _QueueManager();

            //Validate current state
            pInstance->startup();

            //
            shared_ptr<PosixThreadFactory> threadFactory =
                shared_ptr<PosixThreadFactory>(new PosixThreadFactory());

            shared_ptr<Thread> thread =
                threadFactory->newThread(shared_ptr<_QueueManager>(pInstance,null_deleter()));

            thread->start();
        }
    }

    return pInstance;
}

void _QueueManager::startup()
{
    if(started)
        return;

    string doc_root    = ConfigManager->read<string>("DOC_ROOT");

    if( !directory_exists(doc_root) )
        throw std::runtime_error("DOC_ROOT is not valid (check config)");

    started = true;

}

void _QueueManager::run()
{
    map<string, shared_ptr<Queue> >::iterator it;

    while(true){

        for(it=queue_cache.begin(); it!=queue_cache.end(); ++it){
            it->second->pruneLogFile();
        }

        sleep(10);
    }
}


void  _QueueManager::createQueue ( const std::string &id)
{
    Guard g(mutex);

    if( queue_cache.count(id) == 0 )
        queue_cache[id] = shared_ptr<Queue>(new Queue(id));

}

void  _QueueManager::deleteQueue( const std::string &id )
{
    Guard g(mutex);

    if( queue_cache.count(id) > 0 )
        queue_cache.erase(id);

}

shared_ptr<Queue> _QueueManager::getQueue( const std::string &id )
{
    Guard g(mutex);

    if( queue_cache.count(id) > 0 )
        return queue_cache[id];

    ThruqueueException e;
    e.what = "queue not found: "+id;

    throw e;
}
