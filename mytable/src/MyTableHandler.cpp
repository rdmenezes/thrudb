
#include "MyTableHandler.h"
#include "ConfigFile.h"

/*
 * TODO:
 * - input validation, tablenames, key length, value size, etc.
 */

using namespace boost;
using namespace log4cxx;

LoggerPtr MyTableHandler::logger (Logger::getLogger ("MyTableHandler"));

MyTableHandler::MyTableHandler (shared_ptr<MyTableBackend> backend)
{
    this->backend = backend;
}

void MyTableHandler::getTablenames (vector<string> & _return)
{
    LOG4CXX_DEBUG (logger, "getTablenames: ");
    _return = this->backend->getTablenames ();
}

void MyTableHandler::put (const string & tablename, const string & key,
                          const string & value)
{
    LOG4CXX_DEBUG (logger, "put: tablename=" + tablename + ", key=" + key +
                   ", value=" + value);
    this->backend->put (tablename, key, value);
}

void MyTableHandler::get (string & _return, const string & tablename,
                          const string & key)
{
    LOG4CXX_DEBUG (logger, "get: tablename=" + tablename + ", key=" + key);
    _return = this->backend->get (tablename, key);
}

void MyTableHandler::remove (const string & tablename, const string & key)
{
    LOG4CXX_DEBUG (logger, "remove: tablename=" + tablename + ", key=" + key);
    this->backend->remove (tablename, key);
}

void MyTableHandler::scan (ScanResponse & _return, const string & tablename,
                           const string & seed, int32_t count)
{
    {
        char buf[256];
        sprintf (buf, "scan: tablename=%s, seed=%s, count=%d",
                 tablename.c_str (), seed.c_str (), count);
        LOG4CXX_DEBUG (logger, buf);
    }
    _return = this->backend->scan (tablename, seed, count);
}

void MyTableHandler::admin (string & _return, const string & op, const string & data)
{
    LOG4CXX_DEBUG (logger, "admin: op=" + op + ", data=" + data);
    if (op == "echo")
    {
        // echo is a special admin command that we'll handle at this level,
        // everything else will get passed on down
        _return = data;
    }
    else
    {
        _return = this->backend->admin (op, data);
    }
}
