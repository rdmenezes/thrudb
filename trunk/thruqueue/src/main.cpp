/**
 * Copyright (c) 2007- T Jake Luciani
 * Distributed under the New BSD Software License
 *
 * See accompanying file LICENSE or visit the Thrudb site at:
 * http://thrudb.googlecode.com
 *
 **/
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
#include <stdlib.h>

#include "ThruqueueHandler.h"
#include "QueueManager.h"
#include "ConfigFile.h"
#include "utils.h"
#include "ThruLogging.h"

using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::concurrency;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::server;

using namespace boost;
using namespace thruqueue;



//print usage and die
inline void usage()
{
    cerr<<"thruqueue -f /path/to/config.inf"<<endl;
    cerr<<"\tor create ~/.thruqueue"<<endl;
    exit(-1);
}


int main(int argc, char **argv) {

    string conf_file   = string(getenv("HOME"))+"/.thruqueue";
    bool   nonblocking = true;

    //Parse args
    for(int i=0; i<argc; i++){
        if(string(argv[i]) == "-f" && (i+1) < argc)
            conf_file = argv[i+1];
    }

    if( !file_exists( conf_file ) ){
        usage();
    }

    try{
        //Read da config
        ConfigManager->readFile( conf_file );

        int    thread_count = ConfigManager->read<int>("THREAD_COUNT");
        int    server_port  = ConfigManager->read<int>("SERVER_PORT");

        QueueManager->startup();

        T_INFO("Starting up");

        shared_ptr<TProtocolFactory>  protocolFactory  (new TBinaryProtocolFactory());
        shared_ptr<ThruqueueHandler>  handler          (new ThruqueueHandler());
        shared_ptr<TProcessor>        processor        (new ThruqueueProcessor(handler));


        shared_ptr<ThreadManager> threadManager =
            ThreadManager::newSimpleThreadManager(thread_count);

        shared_ptr<PosixThreadFactory> threadFactory =
            shared_ptr<PosixThreadFactory>(new PosixThreadFactory());

        threadManager->threadFactory(threadFactory);
        threadManager->start();


        if(nonblocking){

            TNonblockingServer server(processor,
                                      protocolFactory,
                                      server_port,threadManager);

            cerr<<"Starting the server...\n";
            server.serve();
            cerr<<"Server stopped."<<endl;

        } else {

            shared_ptr<TServerTransport>  serverTransport (new TServerSocket(server_port));
            shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());

            TThreadPoolServer server(processor,
                                     serverTransport,
                                     transportFactory,
                                     protocolFactory,
                                     threadManager);

            cerr<<"Starting the server...\n";
            server.serve();
            cerr<<"Server stopped."<<endl;
        }

    }catch(std::runtime_error e){
        cerr<<"Caught Fatal Exception: "<<e.what()<<endl;
    }catch(std::exception e){
        cerr<<"Caught Fatal Exception: "<<e.what()<<endl;
    }catch(ConfigFile::file_not_found e){
        cerr<<"ConfigFile Fatal Exception: "<<e.filename<<endl;
    }catch(ConfigFile::key_not_found e){
        cerr<<"ConfigFile Missing Required Key: "<<e.key<<endl;
    }catch(...){
        cerr<<"Caught unknown exception"<<endl;
    }

    //QueueManager->destory();

    return 0;
}
