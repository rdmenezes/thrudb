#ifdef HAVE_CONFIG_H
#include "thrudoc_config.h"
#endif
/* hack to work around thrift and log4cxx installing config.h's */
#undef HAVE_CONFIG_H

#include "app_helpers.h"
#include "ConfigFile.h"
#include "BDBBackend.h"
#include "s3_glue.h"
#include "BloomBackend.h"
#include "DiskBackend.h"
#include "LogBackend.h"
#include "MemcachedBackend.h"
#include "MySQLBackend.h"
#include "NBackend.h"
#include "NullBackend.h"
#include "S3Backend.h"
#include "SpreadBackend.h"
#include "ReplicationBackend.h"
#include "StatsBackend.h"
#include "ThrudocBackend.h"

#include "ThruLogging.h"

using namespace boost;
using namespace std;




vector<string> split (const string & str, const string & delimiters = " ")
{
    // Skip delimiters at beginning.
    string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    string::size_type pos     = str.find_first_of(delimiters, lastPos);

    vector<string> tokens;
    while (string::npos != pos || string::npos != lastPos)
    {
        // Found a token, add it to the vector.
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        // Skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);
        // Find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }
    return tokens;
}


shared_ptr<ThrudocBackend> create_backend (string which, int thread_count)
{
    vector<shared_ptr<ThrudocBackend> > backends;

    vector<string> whiches = split (which);
    vector<string>::iterator be;
    for (be = whiches.begin (); be != whiches.end (); be++)
    {
        if ((*be) == "null")
        {
            // NULL backend
            backends.push_back
                (shared_ptr<ThrudocBackend>(new NullBackend ()));
        }
#if HAVE_BERKELEYDB
        if ((*be) == "bdb")
        {
            // BDB backend
            string bdb_home =
                ConfigManager->read<string>("BDB_HOME", "/tmp/bdbs");
            backends.push_back
                (shared_ptr<ThrudocBackend>(new BDBBackend (bdb_home,
                                                            thread_count)));
        }
#endif /* HAVE_BERKELEYDB */
        if ((*be) == "disk")
        {
            // Disk backend
            string doc_root =
                ConfigManager->read<string>("DISK_DOC_ROOT", "/tmp/docs");
            backends.push_back
                (shared_ptr<ThrudocBackend>(new DiskBackend (doc_root)));
        }
#if HAVE_LIBEXPAT && HAVE_LIBCURL
        if ((*be) == "s3")
        {
            // S3 backend
            curl_global_init(CURL_GLOBAL_ALL);

            // TODO: make these part of the backend, so that they're not global
            //s3_debug = 4;
            aws_access_key_id     = ConfigManager->read<string>("AWS_ACCESS_KEY").c_str();
            aws_secret_access_key = ConfigManager->read<string>("AWS_SECRET_ACCESS_KEY").c_str();


            string bucket_prefix =
                ConfigManager->read<string>("S3_BUCKET_PREFIX", "");

            backends.push_back
                (shared_ptr<ThrudocBackend>(new S3Backend (bucket_prefix)));
        }
#endif /* HAVE_LIBEXPAT && HAVE_LIBCURL */
#if HAVE_MYSQL
        if ((*be) == "mysql")
        {
            // MySQL backend
            string master_hostname =
                ConfigManager->read<string>("MYSQL_MASTER_HOST", "localhost");
            short master_port =
                ConfigManager->read<short>("MYSQL_MASTER_PORT", 3306);
            string slave_hostname =
                ConfigManager->read<string>("MYSQL_SLAVE_HOST", "");
            short slave_port =
                ConfigManager->read<short>("MYSQL_SLAVE_PORT", 3306);
            string directory_db =
                ConfigManager->read<string>("MYSQL_DIRECTORY_DB", "thrudoc");
            string username =
                ConfigManager->read<string>("MYSQL_USERNAME", "thrudoc");
            string password =
                ConfigManager->read<string>("MYSQL_PASSWORD", "thrudoc");
            int max_value_size =
                ConfigManager->read<int>("MYSQL_MAX_VALUES_SIZE", 1024);

            backends.push_back (shared_ptr<ThrudocBackend>
                                (new MySQLBackend (master_hostname,
                                                   master_port,
                                                   slave_hostname,
                                                   slave_port,
                                                   directory_db,
                                                   username, password,
                                                   max_value_size)));
        }
#endif /* HAVE_MYSQL */
    }

    shared_ptr<ThrudocBackend> backend;
    if (backends.size () == 0)
    {
        T_ERROR_ABORT ("unknown or unbuilt backend=%s",which.c_str());
    }
    else if (backends.size () == 1)
    {
        backend = *backends.begin ();
    }
    else
    {
        backend = shared_ptr<ThrudocBackend> (new NBackend (backends));
    }

    // Memcached cache
    string memcached_servers =
        ConfigManager->read<string>("MEMCACHED_SERVERS", "");
#if HAVE_LIBMEMCACHED
    if (!memcached_servers.empty ())
        backend = shared_ptr<ThrudocBackend>
            (new MemcachedBackend (backend, memcached_servers));
#else
    if (!memcached_servers.empty ())
    {
        T_ERROR_ABORT ("MEMCACHED_SERVERS supplied, but memcached support not complied in");
    }
#endif /* HAVE_LIBMEMCACHED */

    // Spread passthrough
    string spread_private_name =
        ConfigManager->read<string>("SPREAD_PRIVATE_NAME", "");
    // Spread replication (sorta) passthrough
    string replication_private_name =
        ConfigManager->read<string>("REPLICATION_PRIVATE_NAME", "");
#if HAVE_LIBSPREAD
    string spread_name =
        ConfigManager->read<string>("SPREAD_NAME", "4803");
    string spread_group =
        ConfigManager->read<string>("SPREAD_GROUP", "thrudoc");

    if (!spread_private_name.empty ())
        backend = shared_ptr<ThrudocBackend>
            (new SpreadBackend (backend, spread_name, spread_private_name,
                                spread_group));

    string replication_name =
       ConfigManager->read<string>("REPLICATION_NAME", "4803");
    string replication_group =
        ConfigManager->read<string>("REPLICATION_GROUP", "thrudoc");
    string replication_status_file =
        ConfigManager->read<string>("REPLICATION_STATUS_FILE",
                                    "replication_status");
    int replication_status_flush_frequency =
        ConfigManager->read<int>("REPLICATION_STATUS_FLUSH_FREQUENCY", 30);

    if (!replication_private_name.empty ())
        backend = shared_ptr<ThrudocBackend>
            (new ReplicationBackend (backend, replication_name,
                                     replication_private_name,
                                     replication_group,
                                     replication_status_file,
                                     replication_status_flush_frequency));
#else
    if (!spread_private_name.empty ())
    {
        T_ERROR_ABORT("SPREAD_PRIVATE_NAME supplied, but spread support not complied in");
    }
    if (!replication_private_name.empty ())
    {
        T_ERROR_ABORT("REPLICATION_PRIVATE_NAME supplied, but spread support not complied in");
    }
#endif /* HAVE_LIBSPREAD */

    if(ConfigManager->read<bool>("ENABLE_BLOOM_FILTER",false))
        backend = shared_ptr<ThrudocBackend>(new BloomBackend(backend));


    if (ConfigManager->read<int>("KEEP_STATS", 0))
        backend = shared_ptr<ThrudocBackend> (new StatsBackend (backend));

    // NOTE: logging should always be the outtermost backend
    string log_directory =
        ConfigManager->read<string>("LOG_DIRECTORY","");
    if(!log_directory.empty())
    {
        int max_ops = ConfigManager->read<int>("LOG_MAX_OPS", 25000);
        int sync_wait = ConfigManager->read<int>("LOG_SYNC_WAIT", 5000000);
        backend = shared_ptr<ThrudocBackend> (new LogBackend (backend,
                                                              log_directory,
                                                              max_ops,
                                                              sync_wait));
    }


    return backend;
}

