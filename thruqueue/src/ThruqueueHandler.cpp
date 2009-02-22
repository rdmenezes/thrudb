/**
 * Copyright (c) 2007- T Jake Luciani
 * Distributed under the New BSD Software License
 *
 * See accompanying file LICENSE or visit the Thrudb site at:
 * http://thrudb.googlecode.com
 *
 **/
#include "ThruqueueHandler.h"

#include "QueueManager.h"

using namespace boost;
using namespace thruqueue;

void ThruqueueHandler::createQueue(const std::string& queue_name)
{
    QueueManager->createQueue(queue_name);
}

void ThruqueueHandler::deleteQueue(const std::string& queue_name)
{
    QueueManager->deleteQueue(queue_name);
}

void ThruqueueHandler::listAllQueues(std::vector<std::string> &_return)
{


}

void ThruqueueHandler::sendMessage(const std::string& queue_name, const std::string& mess)
{
    shared_ptr<Queue> queue = QueueManager->getQueue(queue_name);



    queue->sendMessage(mess);
}

void ThruqueueHandler::readMessage(QueueMessage& _return, const std::string& queue_name, int32_t lock_secs)
{
    shared_ptr<Queue> queue = QueueManager->getQueue(queue_name);

    _return = queue->readMessage(lock_secs);
}

void ThruqueueHandler::deleteMessage(const std::string &queue_name, const std::string &message_id)
{
    shared_ptr<Queue> queue = QueueManager->getQueue(queue_name);

    queue->deleteMessage(message_id);
}

void ThruqueueHandler::clearQueue(const std::string& queue_name)
{
    shared_ptr<Queue> queue = QueueManager->getQueue(queue_name);

    queue->clear();

}


int32_t ThruqueueHandler::queueLength(const std::string& queue_name)
{
    shared_ptr<Queue> queue = QueueManager->getQueue(queue_name);

    return queue->length();
}


void ThruqueueHandler::admin(std::string &_return, const std::string &op, const std::string &data)
{


}
