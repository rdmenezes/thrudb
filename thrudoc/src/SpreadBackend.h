/**
 *
 **/

#ifndef _SPREAD_BACKEND_H_
#define _SPREAD_BACKEND_H_

#if HAVE_LIBSPREAD

#include "Thrudoc.h"
#include "ThrudocPassthruBackend.h"

#include <protocol/TBinaryProtocol.h>
#include <set>
#include <Spread.h>
#include <string>
#include <transport/TTransportUtils.h>

class SpreadBackend : public ThrudocPassthruBackend
{
    public:
        SpreadBackend (boost::shared_ptr<ThrudocBackend> backend,
                       const std::string & spread_name,
                       const std::string & spread_private_name,
                       const std::string & spread_group);

        void put (const std::string & bucket, const std::string & key,
                  const std::string & value);
        void remove (const std::string & bucket, const std::string & key);
        std::string admin (const std::string & op, const std::string & data);

    private:
        Spread spread;
        std::string spread_group;

        std::string generate_uuid ();
};

#endif /* HAVE_LIBSPREAD */

#endif /* _SPREAD_BACKEND_H_ */
