#include "MySQLBackend.h"
#include <openssl/md5.h>

LoggerPtr MySQLBackend::logger (Logger::getLogger ("MySQLBackend"));

MySQLBackend::MySQLBackend ()
{
    LOG4CXX_ERROR(logger, "MySQLBackend ()");
}

MySQLBackend::~MySQLBackend ()
{
    // TODO:
}

string MySQLBackend::get (const string & tablename, const string & key )
{
    FindReturn find_return = this->find_and_checkout (tablename, key);
    Connection * connection = find_return.connection;

    PreparedStatement * get_statement = 
        connection->find_get_statement(find_return.data_tablename.c_str ());

    KeyParams * tkp = (KeyParams*)get_statement->get_bind_params ();
    tkp->set_key (key.c_str ());

    get_statement->execute ();

    char buf[1024];

    string value;
    int ret = get_statement->fetch ();
    if (ret == 0)
    {
        KeyValueResults * kvr = 
            (KeyValueResults*)get_statement->get_bind_results ();
        sprintf(buf, "result:\n\tkey:        %s\n\tvalue:      %s\n\tcreated_at: %s\n\tmodifed_at: %s", 
                kvr->get_key (),
                kvr->get_value (), "U", "U");
        LOG4CXX_ERROR(logger,buf);
        value = kvr->get_value ();
    }
    else if (ret == MYSQL_NO_DATA)
    {
        LOG4CXX_ERROR(logger, "we didn't get k,v data");
        MyTableException e;
        e.what = tablename + " " + key + " not found";
        throw e;
    }
    else
    {
        // TODO: ERROR
    }

    this->checkin (find_return.connection);

    // TODO: this will blow things up if return is null
    return value;
}

void MySQLBackend::put (const string & tablename, const string & key, const string & value)
{
    FindReturn find_return = this->find_and_checkout (tablename, key);
    Connection * connection = find_return.connection;

    PreparedStatement * put_statement = 
        connection->find_put_statement (find_return.data_tablename.c_str ());

    KeyValueParams * kvp = (KeyValueParams*)put_statement->get_bind_params ();
    kvp->set_key (key.c_str ());
    kvp->set_value (value.c_str ());

    put_statement->execute ();

    this->checkin (find_return.connection);
}

void MySQLBackend::remove (const string & tablename, const string & key )
{
    FindReturn find_return = this->find_and_checkout (tablename, key);
    Connection * connection = find_return.connection;

    PreparedStatement * delete_statement = 
        connection->find_delete_statement (find_return.data_tablename.c_str ());

    KeyParams * kvp = (KeyParams*)delete_statement->get_bind_params ();
    kvp->set_key (key.c_str ());

    delete_statement->execute ();

    this->checkin (find_return.connection);
}

vector<string> MySQLBackend::scan (const string & tablename, 
                                   const string & seed, int32_t count)
{
    // TODO: ...
    vector<string> list;
    return list;
}

FindReturn MySQLBackend::find_and_checkout (const string & tablename, 
                                            const string & key)
{
    Connection * connection = Connection::checkout ("localhost", "mytable2");

    PreparedStatement * find_statement = 
        connection->find_find_statement (tablename.c_str ());

    // we partition by the md5 of the key so that we'll get an even distrobution 
    // of keys across partitions.
    unsigned char md5[16];
    memset (md5, 0, sizeof (md5));
    MD5((const unsigned char *)key.c_str (), key.length (), md5);
    string md5key;
    char hex[3];
    for (int i = 0; i < 16; i++)
    {
        sprintf (hex, "%02x", md5[i]);
        md5key += string (hex);
    }
    LOG4CXX_ERROR(logger, string ("key=") + key + string (" -> md5key=") +
                  md5key);

    KeyParams * fpp = (KeyParams*)find_statement->get_bind_params ();
    fpp->set_key (md5key.c_str ());

    find_statement->execute ();

    if (find_statement->num_rows () != 1)
    {
        // TODO: error
        LOG4CXX_ERROR(logger, "we didn't get back 1 row");
    }

    bool same_connection = false;
    int ret = find_statement->fetch ();
    FindReturn find_return;
    if (ret == 0)
    {
        PartitionsResults * fpr = 
            (PartitionsResults*)find_statement->get_bind_results ();

        same_connection = connection->is_same (fpr->get_host (), 
                                               fpr->get_db ());
        if (same_connection)
        {
            find_return.connection = connection;
        }
        else
        {
            find_return.connection = Connection::checkout (fpr->get_host (),
                                                           fpr->get_db ());
        }
        find_return.data_tablename = fpr->get_tbl ();

        LOG4CXX_ERROR(logger, string ("data_tablename=") + 
                      find_return.data_tablename);

        char buf[1024];
        sprintf(buf, "result:\n\tid:         %d\n\tstart:      %s\n\tend:        %s\n\thost:       %s\n\tdb:         %s\n\ttbl:        %s\n\test_size:   %d\n\tcreated_at: %s\n\tretired_at: %s", 
                (int)fpr->get_id (),
                fpr->get_start (),
                fpr->get_end (),
                fpr->get_host (),
                fpr->get_db (),
                fpr->get_tbl (),
                fpr->get_est_size (), "U", "U"
                /*
                   !bp.created_at_is_null ? bp.created_at : "nil",
                   !bp.retired_at_is_null ? bp.retired_at : "nil"
                   */
               );
        LOG4CXX_ERROR(logger,buf);
    }
    else if (ret == MYSQL_NO_DATA)
    {
        LOG4CXX_ERROR(logger, "we didn't get back row data");
    }
    else
    {
        // TODO: ERROR
    }

    // if we're not returning our connection, check it back in
    if (!same_connection)
        Connection::checkin (connection);

    return find_return;
}
    
void MySQLBackend::checkin (Connection * connection)
{
    Connection::checkin (connection);
}