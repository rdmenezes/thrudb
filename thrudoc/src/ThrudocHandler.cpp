
#ifdef HAVE_CONFIG_H
#include "thrudoc_config.h"
#endif
/* hack to work around thrift installing config.h's */
#undef HAVE_CONFIG_H

#include "ThrudocHandler.h"
#include "ThruLogging.h"

#include <uuid/uuid.h>


using namespace boost;
using namespace thrudoc;
using namespace std;


ThrudocHandler::ThrudocHandler (shared_ptr<ThrudocBackend> backend)
{
    T_DEBUG ("ThrudocHandler");
    this->backend = backend;
}

void ThrudocHandler::getBuckets (vector<string> & _return)
{
    T_DEBUG ( "getBuckets: ");
    _return = this->backend->getBuckets ();
}

void ThrudocHandler::put (const string & bucket, const string & key,
                          const string & value)
{
    T_DEBUG ("put: bucket=%s, key=%s, value=%s",bucket.c_str(),key.c_str(),value.c_str());
    this->backend->validate (bucket, &key, &value);
    this->backend->put (bucket, key, value);
}

void ThrudocHandler::putValue (string & _return, const string & bucket,
                               const string & value)
{
    uuid_t uuid;
    uuid_generate(uuid);
    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);
    _return = string (uuid_str);

    this->backend->put (bucket, _return, value);
}

void ThrudocHandler::get (string & _return, const string & bucket,
                          const string & key)
{
    T_DEBUG ("get: bucket=%s key=%s",bucket.c_str(), key.c_str());
    this->backend->validate (bucket, &key, NULL);
    _return = this->backend->get (bucket, key);
}

void ThrudocHandler::remove (const string & bucket, const string & key)
{
    T_DEBUG ("remove: bucket=%s key=%s",bucket.c_str(), key.c_str());

    this->backend->validate (bucket, &key, NULL);
    this->backend->remove (bucket, key);
}

void ThrudocHandler::scan (ScanResponse & _return, const string & bucket,
                           const string & seed, int32_t count)
{

    T_DEBUG ("remove: bucket=%s seed=%s count=%d",bucket.c_str(), seed.c_str(), count);

    this->backend->validate (bucket, NULL, NULL);
    _return = this->backend->scan (bucket, seed, count);
}

void ThrudocHandler::admin (string & _return, const string & op, const string & data)
{
    T_DEBUG ("admin: op=%s data=%s",op.c_str(), data.c_str());
    _return = this->backend->admin (op, data);
}

void ThrudocHandler::putList(vector<ThrudocException> & _return,
                             const vector<Element> & elements)
{

    T_DEBUG("putList: elements.size=%d", (int)elements.size ());

    _return = this->backend->putList (elements);
}

void ThrudocHandler::getList(vector<ListResponse> & _return,
                             const vector<Element> & elements)
{
    T_DEBUG("getList: elements.size=%d", (int)elements.size ());

    _return = this->backend->getList (elements);
}

void ThrudocHandler::removeList(vector<ThrudocException> & _return,
                                const vector<Element> & elements)
{
    T_DEBUG("removeList: elements.size=%d", (int)elements.size ());

    _return = this->backend->removeList (elements);
}

void ThrudocHandler::putValueList(vector<ListResponse> & _return,
                                  const vector<Element> & elements)
{

    T_DEBUG("putValueList: elements.size=%d", (int)elements.size ());

    uuid_t uuid;
    char uuid_str[37];
    vector<Element> e = (vector<Element>)elements;
    vector<Element>::iterator i;
    for (i = e.begin (); i != e.end (); i++)
    {
        uuid_generate(uuid);
        uuid_unparse_lower(uuid, uuid_str);
        (*i).key = uuid_str;
    }
    vector<ThrudocException> exceptions = this->backend->putList (elements);
    for (size_t j = 0; j < exceptions.size(); j++)
    {
        ListResponse list_response;
        list_response.ex = exceptions[j];
        list_response.element = elements[j];
        _return.push_back (list_response);
    }
}

