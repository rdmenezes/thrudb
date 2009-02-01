/**
 * Copyright (c) 2007- T Jake Luciani
 * Distributed under the New BSD Software License
 *
 * See accompanying file LICENSE or visit the Thrudb site at:
 * http://thrudb.googlecode.com
 *
 **/

#ifdef HAVE_CONFIG_H
#include "thrucommon_config.h"
#endif
#undef HAVE_CONFIG_H

#include "FileLogger.h"
#include "utils.h"

#include <stdexcept>
#include <sys/time.h>

namespace fs = boost::filesystem;
using namespace boost;
using namespace apache::thrift;
using namespace apache::thrift::concurrency;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;

using namespace std;


FileLogger::FileLogger (const string & log_directory, const string & log_prefix,
                        unsigned int max_ops, unsigned int sync_wait)
{

  T_DEBUG("FileLogger: log_directory=%s, log_prefix=%s, max_ops=%u, sync_wait=%u",
          log_directory.c_str (), log_prefix.c_str (), max_ops,
          sync_wait);


    this->log_directory = log_directory;
    this->log_prefix = log_prefix;
    this->num_ops = 0;
    this->max_ops = max_ops;
    this->sync_wait = sync_wait;

    // if our log directory doesn't exist, create it
    if (!fs::is_directory (log_directory))
    {
        try
        {
            fs::create_directories (log_directory);
        }
        catch (std::exception e)
        {
            T_ERROR ("log error: %s", e.what ());
            throw e;
        }
    }

    // try opening up the index
    index_file.open (log_directory + "/" + log_prefix.c_str() + "index", ios::in);

    // get the last log file name
    T_DEBUG ("reading index");
    string old_log_filename;
    while (index_file.good ())
    {
        char row[256];
        index_file.getline (row, 256);
        if (strlen (row))
        {
            T_DEBUG ("    log file=%s", row);
            old_log_filename = string (row);
        }
    }
    // we're done reading
    index_file.close ();
    T_DEBUG("old_log_filename=%s", old_log_filename.c_str());

    // our new logfile
    string new_log_filename = get_log_filename ();
    T_DEBUG("new_log_filename=%s", new_log_filename.c_str());

    // if there's an old log
    if (!old_log_filename.empty ())
    {
        // open the old log
        open_log_client (old_log_filename, true);
        // write a nextLog of the new logfile so that replayers can chain
        send_nextLog (new_log_filename);
    }
    // then open the new log
    open_log_client (new_log_filename, false);

    // write the new logfile to the index
    index_file.open (log_directory + "/" + log_prefix.c_str () + "index",
                         ios::out | ios::app);
    if (!index_file.is_open ())
    {
        EventLogException e;
        e.what = "error opening log index file";
        T_ERROR (e.what);
        throw e;
    }

    index_file.write ((new_log_filename + "\n").c_str (),
                      new_log_filename.length () + 1);

    // make sure our addition makes it to disk
    index_file.flush ();
}

FileLogger::~FileLogger ()
{
    log_transport->close ();
    index_file.close ();
}

string FileLogger::get_log_filename ()
{
    char buf[64];
    sprintf (buf, "%s%d", log_prefix.c_str (), (int)time (NULL));
    return buf;
}

void FileLogger::open_log_client (string log_filename, bool imediate_sync)
{
    T_DEBUG ("open_log_client: log_filename=%s, imediate_sync=%s", log_filename.c_str(),  (imediate_sync ? "true" : "false"));

    // flush old log file
    if (log_transport)
        log_transport->flush ();

    // and open up a new one
    log_transport = shared_ptr<ThruFileWriterTransport>
        (new ThruFileWriterTransport (log_directory + "/" + log_filename,
                                      imediate_sync ? 0 : this->sync_wait));
    shared_ptr<TProtocol> log_protocol (new TBinaryProtocol (log_transport));
    log_client = shared_ptr<EventLogClient> (new EventLogClient (log_protocol));
}


Event FileLogger::create_event (const string & message)
{
    Event event;

#define NS_PER_S 1000000000LL
#if defined(HAVE_CLOCK_GETTIME)
    struct timespec now;
    int ret = clock_gettime (CLOCK_REALTIME, &now);
    assert (ret == 0);
    event.timestamp = (now.tv_sec * NS_PER_S) + now.tv_nsec;
#elif defined(HAVE_GETTIMEOFDAY)
#define US_PER_NS 1000LL
    struct timeval now;
    int ret = gettimeofday (&now, NULL);
    assert (ret == 0);
    event.timestamp = (((int64_t)now.tv_sec) * NS_PER_S) +
        (((int64_t)now.tv_usec) * US_PER_NS);
#else
#error "one of either clock_gettime or gettimeofday required for FileLogger"
#endif // defined(HAVE_GETTIMEDAY)

    event.message = message;

    return event;
}

void FileLogger::send_nextLog (string new_log_filename)
{
    log_client->send_nextLog (new_log_filename);
}

void FileLogger::send_log (string raw_message)
{
    log_client->send_log (this->create_event (raw_message));

    // this is going to be fuzzy b/c of multi-thread, but that's ok
    this->num_ops++;

    if (this->num_ops >= this->max_ops)
    {
        Guard g(log_mutex);
        if (this->num_ops >= this->max_ops)
        {
            this->roll_log ();
            this->num_ops = 0;
        }
    }
}

void FileLogger::roll_log ()
{
    string new_log_filename = get_log_filename ();
    T_DEBUG("roll_log: new logfile=%s", new_log_filename.c_str());

    // time for a new file
    // point to it in the old one
    send_nextLog (new_log_filename);
    // and open the new one
    open_log_client (new_log_filename, false);
    // add it to the index
    index_file.write ((new_log_filename + "\n").c_str (),
                      new_log_filename.length () + 1);
    index_file.flush ();
}


