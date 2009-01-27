#ifdef HAVE_CONFIG_H
#include "thrudoc_config.h"
#endif
/* hack to work around thrift installing config.h's */
#undef HAVE_CONFIG_H

#include "NullBackend.h"

using namespace boost;
using namespace thrudoc;

using namespace std;


NullBackend::NullBackend ()
{
    T_DEBUG("NullBackend");
}

NullBackend::~NullBackend ()
{
}

vector<string> NullBackend::getBuckets ()
{
    vector<string> buckets;
    return buckets;
}

string NullBackend::get (const string & /* bucket */, const string & /* key */)
{
    return "";
}

void NullBackend::put (const string & /* bucket */, const string & /* key */,
                       const string & /* value */)
{
}

void NullBackend::remove (const string & /* bucket */, const string & /* key */)
{
}

ScanResponse NullBackend::scan (const string & /* bucket */,
                                const string & /* seed */, int32_t /* count */)
{
    ScanResponse scan_response;
    return scan_response;
}

string NullBackend::admin (const string & /* op */, const string & /* data */)
{
    return "";
}

void NullBackend::validate (const string & bucket, const string * key,
                            const string * value)
{
    ThrudocBackend::validate (bucket, key, value);
}
