#ifdef HAVE_CONFIG_H
#include "thrudoc_config.h"
#endif
/* hack to work around thrift installing config.h's */
#undef HAVE_CONFIG_H

#include "NBackend.h"
#include "ThruLogging.h"

using namespace boost;
using namespace thrudoc;

using namespace std;

NBackend::NBackend (vector<shared_ptr<ThrudocBackend> > backends)
{

    T_DEBUG("NBackend: backends.size=%d\n", (int)backends.size ());

    this->set_backend (backends[0]);
    this->backends = backends;
}

void NBackend::put (const string & bucket, const string & key,
                    const string & value)
{
    vector<shared_ptr<ThrudocBackend> >::iterator i;
    for (i = backends.begin (); i != backends.end (); i++)
    {
        (*i)->put (bucket, key, value);
    }
}

void NBackend::remove (const string & bucket, const string & key )
{
    vector<shared_ptr<ThrudocBackend> >::iterator i;
    for (i = backends.begin (); i != backends.end (); i++)
    {
        (*i)->remove (bucket, key);
    }
}

string NBackend::admin (const string & op, const string & data)
{
    string ret;
    vector<shared_ptr<ThrudocBackend> >::iterator i;
    for (i = backends.begin (); i != backends.end (); i++)
    {
        ret += (*i)->admin (op, data) + ";";
    }
    return ret;
}

void NBackend::validate (const string & bucket, const string * key,
                         const string * value)
{
    vector<shared_ptr<ThrudocBackend> >::iterator i;
    for (i = backends.begin (); i != backends.end (); i++)
    {
        (*i)->validate (bucket, key, value);
    }
}
