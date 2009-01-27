#include "Replayer.h"

#include <concurrency/ThreadManager.h>
#include <concurrency/PosixThreadFactory.h>
#include <protocol/TBinaryProtocol.h>
#include <server/TSimpleServer.h>
#include <server/TThreadPoolServer.h>
#include <server/TNonblockingServer.h>
#include <transport/TServerSocket.h>
#include <transport/TTransportUtils.h>

#include <iostream>
#include <stdexcept>
#include <sstream>
#include <stdio.h>

#include <boost/shared_ptr.hpp>

#include <EventLog.h>

#include "app_helpers.h"
#include "ConfigFile.h"
#include "LogBackend.h"
#include "ThrudocBackend.h"
#include "ThrudocHandler.h"
#include "ThruFileTransport.h"
#include "ThruLogging.h"

using namespace boost;
using namespace thrudoc;
using namespace facebook::thrift;
using namespace facebook::thrift::concurrency;
using namespace facebook::thrift::protocol;
using namespace facebook::thrift::server;
using namespace facebook::thrift::transport;

using namespace std;


Replayer::Replayer (shared_ptr<ThrudocBackend> backend, string current_filename,
                  uint32_t delay_seconds)
{

    T_INFO("Replayer: current_filename=%s, delay_seconds=%d",
           current_filename.c_str (), delay_seconds);


    this->backend = backend;
    this->current_filename = current_filename;
    this->delay_seconds = delay_seconds;

    shared_ptr<ThrudocHandler> handler = shared_ptr<ThrudocHandler>
        (new ThrudocHandler (this->backend));
    this->processor = shared_ptr<ThrudocProcessor>
        (new ThrudocProcessor (handler));

    this->last_position_flush = 0;
    this->current_position = 0;

    try
    {
        string last_position;
        last_position = this->backend->admin ("get_log_position", "");
        T_INFO ("last_position=" + last_position);
        if (!last_position.empty ())
        {
            int index = last_position.find (":");
            this->nextLog (last_position.substr (0, index));
            this->current_position =
                atol (last_position.substr (index + 1).c_str ());
            // we have a last position
        }
    }
    catch (ThrudocException e)
    {
        T_INFO ("last_position unknown, assuming epoch");
    }
}

void Replayer::log (const Event & event)
{

    T_DEBUG("log: event.timestamp=%ld, event.msg=***",event.timestamp);

    if (event.timestamp <= this->current_position)
    {
        // we're already at or past this event
        T_DEBUG ("    skipping");
        return;
    }

#define NS_PER_S 1000000000LL
    if (this->delay_seconds)
    {
        // we're supposed to be delaying, figure out when the event
        // should happen
        int32_t event_time = (event.timestamp / NS_PER_S) +
            this->delay_seconds;
        // if that time is in the future
        int32_t sleep_time = event_time - time (NULL);
        if (sleep_time > 0)
        {

            T_DEBUG("log: delaying until: %d", event_time);

            // sleep until it
            sleep (sleep_time);
        }
    }

    // do these really have to be shared pointers?
    shared_ptr<TTransport> tbuf
        (new TMemoryBuffer ((uint8_t*)event.message.c_str (),
                            event.message.length (),
                            TMemoryBuffer::COPY));
    shared_ptr<TProtocol> prot = protocol_factory.getProtocol (tbuf);

    try
    {
        processor->process(prot, prot);
    }
    catch (TTransportException& ttx)
    {
        T_ERROR ("log: client transport error what=%s" ,
                 ttx.what ());
        throw ttx;
    }
    catch (TException& x)
    {
        T_ERROR ("log: client error what=%s",
                 x.what ());
        throw x;
    }
    catch (...)
    {
        T_ERROR ("log: client 'other' error");
        throw;
    }

    // write our log position out to "storage" every once in a while,
    // we'll be at most interval behind and thus at most need to replay
    // that many seconds in to the slave datastore
    if (time (NULL) > this->last_position_flush + 60)
    {

        char buf[64];
        sprintf (buf, "%s:%ld", get_current_filename ().c_str (),
                 event.timestamp);


        T_DEBUG(buf);

        this->backend->admin ("put_log_position", buf);
        this->last_position_flush = time (NULL);
    }

    // update the current position
    this->current_position = event.timestamp;
}

void Replayer::nextLog (const string & next_filename)
{
    T_INFO ("nextLog: next_filename=%s",next_filename.c_str());
    this->current_filename = next_filename;
}

string Replayer::get_current_filename ()
{
    return this->current_filename;
}




