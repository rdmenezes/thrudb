/**
 * Copyright (c) 2007- T Jake Luciani
 * Distributed under the New BSD Software License
 *
 * See accompanying file LICENSE or visit the Thrudb site at:
 * http://thrudb.googlecode.com
 *
 **/

#ifndef _THRUDOC_S3_BACKEND_H_
#define _THRUDOC_S3_BACKEND_H_

#if HAVE_LIBEXPAT && HAVE_LIBCURL

#include <string>

#include "Thrudoc.h"
#include "ThrudocBackend.h"

class S3Backend : public ThrudocBackend
{
    public:
        S3Backend (std::string bucket_prefix);

        std::vector<std::string> getBuckets ();
        std::string get (const std::string & bucket,
                         const std::string & key);
        void put (const std::string & bucket, const std::string & key,
                  const std::string & value);
        void remove (const std::string & bucket, const std::string & key);
        thrudoc::ScanResponse scan (const std::string & bucket,
                                    const std::string & seed, int32_t count);
        std::string admin (const std::string & op, const std::string & data);
        void validate (const std::string & bucket, const std::string * key,
                       const std::string * value);

    private:

        std::string bucket_prefix;
};

#endif /* HAVE_LIBEXPAT && HAVE_LIBCURL */

#endif
