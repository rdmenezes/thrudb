/**
 * Copyright (c) 2007- T Jake Luciani
 * Distributed under the New BSD Software License
 *
 * See accompanying file LICENSE or visit the Thrudb site at:
 * http://thrudb.googlecode.com
 *
 **/

#ifdef HAVE_CONFIG_H
#include "thrudex_config.h"
#endif
/* hack to work around thrift and log4cxx installing config.h's */
#undef HAVE_CONFIG_H

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
#include <boost/utility.hpp>

#include <EventLog.h>

#include "app_helpers.h"
#include "ConfigFile.h"
#include "LogBackend.h"
#include "ThrudexBackend.h"
#include "ThrudexHandler.h"
#include "ThrudexHandler.h"
#include "ThruFileTransport.h"

using namespace boost;
using namespace thrudex;
using namespace apache::thrift;
using namespace apache::thrift::concurrency;
using namespace apache::thrift::protocol;
using namespace apache::thrift::server;
using namespace apache::thrift::transport;

using namespace std;

//print usage and die
inline void usage ()
{
    cerr<<"thrudex -f /path/to/thrudex.conf"<<endl;
    cerr<<"\tor create ~/.thrudex"<<endl;
    cerr<<"\t-nb creates non-blocking server"<<endl;
    exit (-1);
}

class Replayer : public EventLogIf
{
    public:
        Replayer (shared_ptr<ThrudexBackend> backend, string current_filename,
                  uint32_t delay_seconds)
        {

            T_DEBUG( "Replayer: current_filename=%s, delay_seconds=%d",
                     current_filename.c_str (), delay_seconds);


            this->backend = backend;
            this->current_filename = current_filename;
            this->delay_seconds = delay_seconds;

            shared_ptr<ThrudexHandler> handler = shared_ptr<ThrudexHandler>
                (new ThrudexHandler (this->backend));
            this->processor = shared_ptr<ThrudexProcessor>
                (new ThrudexProcessor (handler));

            this->last_position_flush = 0;
            this->current_position = 0;

            try
            {
                string last_position;
                last_position = this->backend->admin ("get_log_position", "");
                T_DEBUG( "last_position=%s", last_position.c_str());
                if (!last_position.empty ())
                {
                    int index = last_position.find (":");
                    this->nextLog (last_position.substr (0, index));
                    this->current_position =
                        atol (last_position.substr (index + 1).c_str ());
                    // we have a last position
                }
            }
            catch (ThrudexException e)
            {
                T_ERROR( "last_position unknown, assuming epoch");
            }
        }

        void log (const Event & event)
        {
            T_DEBUG( "log: event.timestamp=%ld, event.msg=***",
                     event.timestamp);

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

                    T_DEBUG( "log: delaying until: %d", event_time);

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
                T_ERROR ("log: client transport error what=%s",ttx.what ());
                throw ttx;
            }
            catch (TException& x)
            {
                T_ERROR("log: client error what=%s",x.what ());
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
                char buf[512];
                sprintf(buf,"%s:%ld", get_current_filename ().c_str (),
                        event.timestamp);

                T_DEBUG( "log: flushing position=%s", buf);

                this->backend->admin ("put_log_position", buf);
                this->last_position_flush = time (NULL);
            }

            // update the current position
            this->current_position = event.timestamp;
        }

        void nextLog (const string & next_filename)
        {
            T_DEBUG("nextLog: next_filename=%s", next_filename.c_str());
            this->current_filename = next_filename;
        }

        string get_current_filename ()
        {
            return this->current_filename;
        }

    private:


        TBinaryProtocolFactory protocol_factory;
        shared_ptr<ThrudexBackend> backend;
        shared_ptr<ThrudexProcessor> processor;
        string current_filename;
        int64_t current_position;
        time_t last_position_flush;
        uint32_t delay_seconds;
};



int main (int argc, char **argv)
{

    string conf_file = string (getenv ("HOME"))+"/.thrudex";
    bool nonblocking = true;

    //Parse args
    for (int i=0; i<argc; i++)
    {
        if (string (argv[i]) == "-f" && (i+1) < argc)
            conf_file = argv[i+1];

        if (string (argv[i]) == "-nb")
            nonblocking = true;
    }

    //Read da config
    ConfigManager->readFile ( conf_file );

    try
    {

        string log_directory = argv[argc-1];
        T_DEBUG("log_directory=%s", log_directory.c_str());

        // open the log index file
        fstream index_file;
        index_file.open ((log_directory + "/" + LOG_FILE_PREFIX +
                          "index").c_str (), ios::in);
        string log_filename;
        if (index_file.good ())
        {
            // read the first line
            char buf[64];
            index_file.getline (buf, 64);
            log_filename = string (buf);
        }
        else
        {
            ThrudexException e;
            e.what = "error opening log index file";
            T_ERROR ( e.what.c_str());
            throw e;
        }

        if (log_filename.empty ())
        {
            ThrudexException e;
            e.what = "error log index file empty";
            T_ERROR (e.what.c_str());
            throw e;
        }

        shared_ptr<TProtocolFactory>
            protocolFactory (new TBinaryProtocolFactory ());

        // create our backend
        string which = ConfigManager->read<string> ("BACKEND", "mysql");
        shared_ptr<ThrudexBackend> backend = create_backend (which, 1);

        int32_t delay_seconds =
            ConfigManager->read<int32_t> ("REPLAY_DELAY_SECONDS", 0);

        // create our replayer with initial log_filename
        boost::shared_ptr<Replayer> replayer (new Replayer (backend,
                                                            log_filename,
                                                            delay_seconds));
        // blank it out so we'll open things up... HACK
        log_filename = "";

        ThruFileProcessor * fileProcessor = NULL;
        while (1)
        {
            // check for new file
            if (log_filename != replayer->get_current_filename ())
            {
                log_filename = replayer->get_current_filename ();

                T_DEBUG ( "opening=%s/%s", log_directory.c_str(),
                          log_filename.c_str());

                // we have to sleep for a little bit here to give the new file
                // time to come in to existence to make sure we don't beat
                // it...
                sleep (1);

                shared_ptr<ThruFileReaderTransport>
                    rlog (new ThruFileReaderTransport (log_directory + "/" +
                                                       log_filename));
                boost::shared_ptr<EventLogProcessor>
                    proc (new EventLogProcessor (replayer));
                shared_ptr<TProtocolFactory> pfactory (new TBinaryProtocolFactory ());

                if (fileProcessor)
                    delete fileProcessor;
                fileProcessor = new ThruFileProcessor (proc, pfactory, rlog);
            }
            fileProcessor->process (1);
        }

    }
    catch (TException e)
    {
        cerr << "Thrift Error: " << e.what () << endl;
    }
    catch (std::runtime_error e)
    {
        cerr << "Runtime Exception: " << e.what () << endl;
    }
    catch (ConfigFile::file_not_found e)
    {
        cerr << "ConfigFile Fatal Exception: " << e.filename << endl;
    }
    catch (ConfigFile::key_not_found e)
    {
        cerr << "ConfigFile Missing Required Key: " << e.key << endl;
    }
    catch (std::exception e)
    {
        cerr << "Caught Fatal Exception: " << e.what () << endl;
    }
    catch (...)
    {
        cerr << "Caught unknown exception" << endl;
    }

    return 0;
}
