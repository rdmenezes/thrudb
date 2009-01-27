/**
 * Copyright (c) 2007- T Jake Luciani
 * Distributed under the New BSD Software License
 *
 * See accompanying file LICENSE or visit the Thrudb site at:
 * http://thrudb.googlecode.com
 *
 **/

#ifdef HAVE_CONFIG_H
#include "thrudoc_config.h"
#endif
/* hack to work around thrift and log4cxx installing config.h's */
#undef HAVE_CONFIG_H


#include <iostream>
#include <stdexcept>
#include <sstream>

#include <concurrency/ThreadManager.h>
#include <concurrency/PosixThreadFactory.h>
#include <protocol/TBinaryProtocol.h>
#include <server/TSimpleServer.h>
#include <server/TThreadPoolServer.h>
#include <server/TNonblockingServer.h>
#include <transport/TServerSocket.h>
#include <transport/TTransportUtils.h>

#include <boost/shared_ptr.hpp>


#include "app_helpers.h"
#include "ConfigFile.h"
#include "LogBackend.h"
#include "ThrudocBackend.h"
#include "ThrudocHandler.h"
#include "ThruFileTransport.h"
#include "ThruLogging.h"
#include "Replayer.h"
#include "Thrudoc.h"

using namespace facebook::thrift;
using namespace facebook::thrift::concurrency;
using namespace facebook::thrift::protocol;
using namespace facebook::thrift::server;
using namespace facebook::thrift::transport;

using namespace std;

//print usage and die
inline void usage ()
{
    cerr<<"thrudoc_replay -f /path/to/thrudoc.conf"<<endl;
    cerr<<"\tor create ~/.thrudoc"<<endl;
    cerr<<"\t-nb creates non-blocking server"<<endl;
    exit (-1);
}


int main (int argc, char **argv)
{

    string conf_file = string (getenv ("HOME"))+"/.thrudoc";
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
        T_INFO ("log_directory=%s", log_directory.c_str());

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
            thrudoc::ThrudocException e;
            e.what = "error opening log index file";
            T_ERROR ( e.what);
            throw e;
        }

        if (log_filename.empty ())
        {
            thrudoc::ThrudocException e;
            e.what = "error log index file empty";
            T_ERROR (e.what);
            throw e;
        }

        boost::shared_ptr<TProtocolFactory>
            protocolFactory (new TBinaryProtocolFactory ());

        // create our backend
        string which = ConfigManager->read<string> ("BACKEND", "mysql");
        boost::shared_ptr<ThrudocBackend> backend = create_backend (which, 1);

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

                T_INFO ("opening=%s/%s", log_directory.c_str(), log_filename.c_str());

                // we have to sleep for a little bit here to give the new file
                // time to come in to existence to make sure we don't beat
                // it...
                sleep (1);

                boost::shared_ptr<ThruFileReaderTransport>
                    rlog (new ThruFileReaderTransport (log_directory + "/" +
                                                       log_filename));
                boost::shared_ptr<EventLogProcessor>
                    proc (new EventLogProcessor (replayer));
                boost::shared_ptr<TProtocolFactory> pfactory (new TBinaryProtocolFactory ());

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
