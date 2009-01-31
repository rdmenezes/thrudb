#include "UpdateFilter.h"
#include "CLuceneIndex.h"
#include "ThruLogging.h"

using namespace boost;
using namespace lucene::index;
using namespace lucene::search;
using namespace lucene::util;

UpdateFilter::UpdateFilter(shared_ptr<IndexReader> reader)
    : reader(reader), num_skipped(0)
{

    bitset = shared_ptr<BitSet>(new BitSet(reader->maxDoc()));

    //Set them all to true
    for(int32_t i=0; i<reader->maxDoc(); i++){
        bitset->set(i,true);
    }
}

UpdateFilter::~UpdateFilter()
{
    T_DEBUG("Deleted update filter");
}

BitSet* UpdateFilter::bits(IndexReader* reader)
{

    T_DEBUG("bits");

    //Disk updates are all that should be filtered
    if(reader == this->reader.get()){

        return bitset.get();
    } else {

        T_DEBUG("creating in mem filter");
        //this is a in memory index no filter
        BitSet *tmp_bs = new BitSet(reader->maxDoc());
        for(int i=0; i<reader->maxDoc(); i++)
            tmp_bs->set(i,true);

        return tmp_bs;
    }
}

void UpdateFilter::skip( wstring key )
{
    TermEnum *enumerator;
    Term              *t;
    try{
        t = new Term(DOC_KEY, key.c_str() );

        enumerator = reader->terms(t);

        if (enumerator->term(false) == NULL){
            _CLDELETE(enumerator);
            delete t;
            return;
        }
    }catch(CLuceneError e){
        cerr<<"!!!Caught Fatal CLucene Exception: "<<e.what()<<endl;
        delete t;
        return;
    }

    TermDocs* termDocs = reader->termDocs();

    try {

        termDocs->seek(enumerator->term(false));
        while (termDocs->next()) {
            bitset->set(termDocs->doc(),false);
        }

    } _CLFINALLY (
        termDocs->close();
        _CLDELETE(termDocs);
        enumerator->close();
        _CLDELETE(enumerator);
        delete t;
    );
}

Filter* UpdateFilter::clone() const
{
    return NULL;
}


bool UpdateFilter::shouldDeleteBitSet(const BitSet* bs ) const
{
    //allow caller to delete tmp bitset only
    return bs == bitset.get() ? false : true;
}


TCHAR* UpdateFilter::toString()
{
    return L"";
}
