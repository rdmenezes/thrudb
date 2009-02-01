/**
 * Copyright (c) 2007- T Jake Luciani
 * Distributed under the New BSD Software License
 *
 * See accompanying file LICENSE or visit the Thrudb site at:
 * http://thrudb.googlecode.com
 *
 **/
#ifndef _THRUDOC_BLOOM_BACKEND_H_
#define _THRUDOC_BLOOM_BACKEND_H_

#include <string>
#include <boost/shared_ptr.hpp>

#include <transport/TTransportUtils.h>
#include <protocol/TBinaryProtocol.h>
#include <concurrency/Mutex.h>

#include "ThrudocPassthruBackend.h"

/**
 *This backend uses bloom filters to quickly disregard requests
 *for keys that do not exist in the store.  This saves on disk/network IO
 *
 *This could/should be altered to kick off a thread that refreshes the bloom
 *filter by scanning its local store...
 **/

class bloom_filter;

class BloomBackend : public ThrudocPassthruBackend
{
 public:
    BloomBackend( boost::shared_ptr<ThrudocBackend> backend );
    ~BloomBackend();

    std::string get (const std::string & bucket,
                     const std::string & key);

    void put (const std::string & bucket, const std::string & key,
              const std::string & value);

 protected:
    bloom_filter *hit_filter;  ///<ids that were found in the store
    bloom_filter *miss_filter; ///<ids that were not found in the store
    bloom_filter *miss_filter2;///<misses are double filtered for extra protection...

    unsigned int filter_space;

    apache::thrift::concurrency::Mutex mutex;
};

#endif
