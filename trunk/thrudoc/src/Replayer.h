#ifndef _THRUDOC_REPLAYER_
#define _THRUDOC_REPLAYER_

#include <boost/shared_ptr.hpp>

#include <EventLog.h>
#include "ThrudocBackend.h"
#include "Thrudoc.h"
#include <protocol/TBinaryProtocol.h>

class Replayer : public EventLogIf
{
    public:
    Replayer (boost::shared_ptr<ThrudocBackend> backend, std::string current_filename,
                  uint32_t delay_seconds);

        void log (const Event & event);
        void nextLog (const std::string & next_filename);
        std::string get_current_filename ();
private:

        apache::thrift::protocol::TBinaryProtocolFactory protocol_factory;
        boost::shared_ptr<ThrudocBackend> backend;
        boost::shared_ptr<thrudoc::ThrudocProcessor> processor;
        std::string current_filename;
        int64_t current_position;
        time_t last_position_flush;
        uint32_t delay_seconds;
};


#endif

