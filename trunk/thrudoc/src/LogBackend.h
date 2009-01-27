/**
 * Copyright (c) 2007- T Jake Luciani
 * Distributed under the New BSD Software License
 *
 * See accompanying file LICENSE or visit the Thrudb site at:
 * http://thrudb.googlecode.com
 *
 **/
#ifndef _THRUDOC_LOG_BACKEND_H_
#define _THRUDOC_LOG_BACKEND_H_

#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>

#include <string>
#include <concurrency/Mutex.h>
#include <protocol/TBinaryProtocol.h>
#include <protocol/TDenseProtocol.h>
#include <transport/TFileTransport.h>
#include <transport/TTransportUtils.h>

#include "ThrudocPassthruBackend.h"

#include <EventLog.h>
#include <FileLogger.h>

#define LOG_FILE_PREFIX "thrudoc-log."

class LogBackend : public ThrudocPassthruBackend
{
    public:
        LogBackend (boost::shared_ptr<ThrudocBackend> backend,
                    const std::string &log_directory, unsigned int max_ops,
                    unsigned int sync_wait);

        void put (const std::string & bucket, const std::string & key,
                  const std::string & value);

        void remove (const std::string & bucket, const std::string & key);

        std::string admin (const std::string & op, const std::string & data);

        std::vector<thrudoc::ThrudocException> putList
            (const std::vector<thrudoc::Element> & elements);
        std::vector<thrudoc::ThrudocException> removeList
            (const std::vector<thrudoc::Element> & elements);

    private:

        // this will be used to create the event message
        boost::shared_ptr<facebook::thrift::transport::TMemoryBuffer> msg_transport;
        boost::shared_ptr<thrudoc::ThrudocClient> msg_client;
        Event create_event (const std::string &msg);

        FileLogger * file_logger;
};

#endif
