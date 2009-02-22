/**
 * Copyright (c) 2007- T Jake Luciani
 * Distributed under the New BSD Software License
 *
 * See accompanying file LICENSE or visit the Thrudb site at:
 * http://thrudb.googlecode.com
 *
 **/

#ifndef __THRUQUEUE_HANDLER_H__
#define __THRUQUEUE_HANDLER_H__

#include "Thruqueue.h"

class ThruqueueHandler : virtual public thruqueue::ThruqueueIf {
 public:
    ThruqueueHandler() {};

    void ping(){};

    void createQueue(const std::string& queue_name);

    void deleteQueue(const std::string& queue_name);

    void listAllQueues(std::vector<std::string> &_return);

    void sendMessage(const std::string& queue_name, const std::string& mess);

    void readMessage(thruqueue::QueueMessage& _return, const std::string& queue_name, int32_t lock_secs);

    void deleteMessage( const std::string& queue_name, const std::string &message_id );

    void clearQueue(const std::string& queue_name);

    int32_t queueLength(const std::string& queue_name);

    void admin(std::string &_return, const std::string &op, const std::string &data);
};

#endif
