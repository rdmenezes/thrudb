#ifdef HAVE_CONFIG_H
#include "thrudex_config.h"
#endif
/* hack to work around thrift and log4cxx installing config.h's */
#undef HAVE_CONFIG_H

#include "CLuceneIndex.h"
#include "utils.h"
#include <concurrency/Util.h>
#include <concurrency/PosixThreadFactory.h>

#include "bloom_filter.hpp"
#include "UpdateFilter.h"
#include "ThruLogging.h"


using namespace thrudex;


using namespace boost;
using namespace facebook::thrift::concurrency;

using namespace lucene::index;
using namespace lucene::store;
using namespace lucene::search;
using namespace lucene::analysis;
using namespace lucene::util;
using namespace lucene::queryParser;
using namespace lucene::document;


//Used to spawn monitor thread
struct null_deleter
{
    void operator()(void const *) const {}
};

//Used to clean up
struct reader_deleter
{
    void operator()(void const *o) const {
        IndexReader *r = (IndexReader *)o;

        T_DEBUG("called reader cleanup");

        r->close();
        delete r;
    }
};

CLuceneIndex::CLuceneIndex(const string &index_root, const string &index_name, const size_t &filter_space, shared_ptr<Analyzer> analyzer)
    : index_root(index_root), index_name(index_name), analyzer(analyzer), filter_space(filter_space), last_synched(0), syncing(false)
{

    //Verify log dir
    if(!directory_exists( index_root )){
        T_ERROR("Invalid index root: %s",index_root.c_str());
        throw runtime_error("Invalid index root: "+index_root);
    }

    string idx_path = index_root + "/" + index_name;
    bool   new_index;

    random_seed = (size_t)filter_space*rand();

    try {

        //existing index?
        if ( IndexReader::indexExists(idx_path.c_str()) ) {


            //Force unlock index incase it was left locked
            if ( IndexReader::isLocked(idx_path.c_str()) )
                IndexReader::unlock(idx_path.c_str());

            //optimize on startup
            this->optimize();

            new_index = false;

        } else {
            standard::StandardAnalyzer a;
            IndexWriter w(idx_path.c_str(),&a,true,true);
            w.close();

            new_index = true;
            T_DEBUG("Created index :%s",index_name.c_str());
        }

        //build up bloom filter
        disk_bloom    = shared_ptr<bloom_filter>(new bloom_filter(filter_space,1.0/(1.0 * filter_space), random_seed));
        disk_reader   = shared_ptr<IndexReader>(IndexReader::open(idx_path.c_str()), reader_deleter() );
        disk_filter   = shared_ptr<UpdateFilter>(new UpdateFilter(disk_reader));

        if(!new_index){

            int max = disk_reader->maxDoc();
            char buf[1024];

            for(int i=0; i<max; i++){
                try{

                    if(disk_reader->isDeleted(i))
                        continue;

                    const wchar_t *id   = disk_reader->document(i)->get( DOC_KEY );

                    STRCPY_TtoA(buf,id,1024);

                    T_DEBUG("blooming index id: %s(%s)",index_name.c_str(),buf);

                    disk_bloom->insert(buf);
                }catch(CLuceneError &e){
                    T_ERROR("Error while populating bloom filter: %s",e.what());
                }
            }
        }

        ram_directory = shared_ptr<CLuceneRAMDirectory>(new CLuceneRAMDirectory());
        ram_directory->__cl_addref(); //trick clucene's lame ref counters

        ram_prev_directory = shared_ptr<CLuceneRAMDirectory> (new CLuceneRAMDirectory());
        ram_prev_directory->__cl_addref(); //trick clucene's lame ref counters

        ram_bloom     = shared_ptr<bloom_filter> (new bloom_filter(filter_space,1.0/(1.0 * filter_space), random_seed));
        ram_searcher  = shared_ptr<IndexSearcher>(new IndexSearcher(ram_directory.get()));

        disk_searcher = shared_ptr<IndexSearcher>(new IndexSearcher(disk_reader.get()));
        last_refresh  = -1;


        modifier      = shared_ptr<IndexModifier>(new IndexModifier(ram_directory.get(),analyzer.get(),true));
        last_modified = 0;

        disk_deletes  = shared_ptr<set<string> >(new set<string>());



    } catch(CLuceneError &e) {
        T_ERROR("Clucene Exception while creating index:%s : %s", idx_path.c_str(),e.what());
        ThrudexException ex;
        ex.what = "Clucene Exception while creating index:" + idx_path +" : "+e.what();

        throw ex;
    } catch(...) {
        T_ERROR("Unknown exception while creating index:%s", idx_path.c_str());

        ThrudexException ex;
        ex.what = "Unknown exception while creating index:" + idx_path;

        throw ex;
    }

    //Kick off the monitor thread
    shared_ptr<PosixThreadFactory> threadFactory =
        shared_ptr<PosixThreadFactory>(new PosixThreadFactory());

    monitor_thread =
        threadFactory->newThread(shared_ptr<CLuceneIndex>(this,null_deleter()));

    monitor_thread->start();
}

CLuceneIndex::~CLuceneIndex()
{
    sync();
}

shared_ptr<MultiSearcher> CLuceneIndex::getSearcher()
{

    //syncronized in the caller
    if(last_refresh < last_modified || last_refresh < last_synched){

        modifier->flush();

        ram_searcher.reset();

        //make a copy of the ram dir since its not thread safe
        ram_readonly_directory.reset( new CLuceneRAMDirectory( ram_directory.get() ), null_deleter() );
        ram_readonly_directory->__cl_addref(); //trick clucene's lame ref counters

        ram_searcher.reset(new IndexSearcher( ram_readonly_directory.get() ));

        //since clucene doesn't use shared_ptr we need to get the
        //underlying ptr
        Searchable *searchers[4];
        searchers[0] = ram_searcher.get();
        searchers[1] = disk_searcher.get();

        if(syncing){
            //make a copy of the ram dir since its not thread safe
          ram_readonly_prev_directory = shared_ptr<CLuceneRAMDirectory>(new CLuceneRAMDirectory( ram_prev_directory.get() ), null_deleter() );
          ram_readonly_prev_directory->__cl_addref(); //trick clucene's lame ref counters

            ram_prev_searcher.reset(new IndexSearcher( ram_readonly_prev_directory.get() ));
            searchers[2] = ram_prev_searcher.get();
            searchers[3] = NULL;
        }else{
            searchers[2] = NULL;
        }

        searcher.reset( new MultiSearcher( searchers ) );

        last_refresh = Util::currentTime();

        T_DEBUG("Created new searcher");
    }

    return searcher;
}


void CLuceneIndex::put( const string &key, lucene::document::Document *doc )
{

    if(key.empty()){
        ThrudexException ex;
        ex.what = "Empty key";
        throw ex;
    }

    //RWGuard g( mutex, true );
    Guard g( mutex );

    //always put into memory (we will merge to disk later)
    shared_ptr<bloom_filter>  l_disk_bloom   = disk_bloom;
    shared_ptr<bloom_filter>  l_ram_bloom    = ram_bloom;
    shared_ptr<IndexModifier> l_modifier     = modifier;
    shared_ptr<set<string> >  l_disk_deletes = disk_deletes;
    shared_ptr<UpdateFilter>  l_disk_filter  = disk_filter;
    shared_ptr<IndexReader>   l_disk_reader  = disk_reader;

    wstring wkey = build_wstring(key);

    //if update to a doc in memory old copy first
    if( ram_bloom->contains( key ) ){

        Term *t = new Term(DOC_KEY, wkey.c_str() );

        l_modifier->deleteDocuments(t);
        T_DEBUG("Updating %s",key.c_str());

        delete t;
    }

    l_modifier->addDocument(doc);
    l_ram_bloom->insert( key );

    //If this exists already on disk remove it
    if( l_disk_bloom->contains( key ) ){
        l_disk_deletes->insert( key );

        if(!syncing)
            l_disk_filter->skip(wkey);
    }

    last_modified = Util::currentTime();
}

void CLuceneIndex::remove(const string &key)
{
    //RWGuard g(mutex, true);
    Guard g(mutex);

    shared_ptr<bloom_filter>  l_disk_bloom   = disk_bloom;
    shared_ptr<bloom_filter>  l_ram_bloom    = ram_bloom;
    shared_ptr<IndexModifier> l_modifier     = modifier;
    shared_ptr<set<string> >  l_disk_deletes = disk_deletes;
    shared_ptr<UpdateFilter>  l_disk_filter  = disk_filter;
    shared_ptr<IndexReader>   l_disk_reader  = disk_reader;

    wstring wkey = build_wstring(key);

    //Since we don't want to write to disk
    //We'll simply track the docs to remove on next merge
    if( l_disk_bloom->contains( key )){
        T_DEBUG("Removed disk %s",key.c_str());
        l_disk_deletes->insert( key );

        if(!syncing)
            l_disk_filter->skip(wkey);

        last_modified = Util::currentTime();
    }

    //remove from memory if residing there
    if(l_ram_bloom->contains( key )){

        T_DEBUG( "Removed ram %s",key.c_str());

        Term      *t = new Term(DOC_KEY, wkey.c_str() );

        l_modifier->deleteDocuments(t);

        last_modified = Util::currentTime();

        delete t;
    }

}


void CLuceneIndex::search(const thrudex::SearchQuery &q, thrudex::SearchResponse &r)
{

    T_DEBUG("Searching in: (%s)",q.index.c_str());


    if( q.query.empty() ){
        ThrudexException ex;
        ex.what = "Query is empty";
        throw ex;
    }

    shared_ptr<CLuceneRAMDirectory> l_ram_readonly_directory;
    shared_ptr<CLuceneRAMDirectory> l_ram_directory;
    shared_ptr<CLuceneRAMDirectory> l_ram_readonly_prev_directory;
    shared_ptr<CLuceneRAMDirectory> l_ram_prev_directory;
    shared_ptr<IndexSearcher>       l_ram_searcher;
    shared_ptr<IndexSearcher>       l_ram_prev_searcher;
    shared_ptr<IndexSearcher>       l_disk_searcher;
    shared_ptr<UpdateFilter>        l_disk_filter;
    shared_ptr<IndexReader>         l_disk_reader;
    shared_ptr<MultiSearcher>       l_searcher;
    //RWGuard g(mutex);
    {
        Guard g(mutex);

        l_searcher    = this->getSearcher();

        //making sure references to underlying objects stay above 0
        //for the duration of this function
        l_ram_readonly_directory      = ram_readonly_directory;
        l_ram_directory               = ram_directory;
        l_ram_readonly_prev_directory = ram_readonly_prev_directory;
        l_ram_prev_directory          = ram_prev_directory;
        l_ram_searcher                = ram_searcher;
        l_ram_prev_searcher           = ram_prev_searcher;
        l_disk_searcher               = disk_searcher;

        l_disk_filter = disk_filter;
        l_disk_reader = disk_reader;
    }

    Query *query;

    try{

        T_DEBUG("%s",q.query.c_str());

        wstring wquery = build_wstring(q.query);

        query = QueryParser::parse( wquery.c_str(),DOC_KEY,analyzer.get());

        if( query == NULL ){
            ThrudexException ex;
            ex.what  = "Invalid query: '"+q.query+"'";

            throw ex;
        }

    } catch(CLuceneError e) {

        ThrudexException ex;
        ex.what  = "Invalid query: '"+string(e.what())+"'";

        throw ex;

    } catch(runtime_error e) {

        ThrudexException ex;
        ex.what  = "Invalid query: '"+string(e.what())+"'";

        throw ex;
    } catch (...) {

        ThrudexException ex;
        ex.what  = "Unknown error while parsing query";

        throw ex;

    }


    Hits *h;
    Sort *lsort = NULL;

    try{

        if( q.sortby.empty() ){
            h = l_searcher->search(query, l_disk_filter.get());
        } else {


            T_DEBUG("Sorting by: %s %s",q.sortby.c_str(),(q.desc ? "Descending": ""));

            wstring  sortby   = build_wstring( q.sortby+"_sort" );

            lsort = new Sort();
            lsort->setSort(  new SortField ( sortby.c_str(), SortField::STRING, q.desc ) );

            try {
                h = l_searcher->search(query,l_disk_filter.get(),lsort);
            } catch(CLuceneError &e) {

                T_INFO( "Sort failed, falling back on regular search");
                h = l_searcher->search(query,l_disk_filter.get());
            }
        }
    }catch(CLuceneError &e){

        ThrudexException ex;
        ex.what  = "Error while performing search: '"+string(e.what())+"'";

        throw ex;

    }


    char buf[1024];

    r.total = h->length();

    int mlen = h->length();

    if( mlen > 0 ){

        if(q.randomize){

            map<int, bool> unique_lookup;

            for(int j=0; j<mlen && j<q.limit; j++){
                int k = 0;

                do{
                    k = rand() % mlen;
                }while(unique_lookup.count(k));

                lucene::document::Document *doc = &h->doc(k);

                const wchar_t *id   = doc->get(DOC_KEY);

                if(id == NULL){

                    T_ERROR("Dockey missing from document!")
                    continue;
                } else {

                    STRCPY_TtoA(buf,id,1024);

                    T_DEBUG("ID: %s",buf);

                    thrudex::Element el;
                    el.index = q.index;
                    el.key   = buf;

                    if(q.payload){
                      const wchar_t *payload = doc->get(DOC_PAYLOAD);
                      if(payload != NULL){
                        STRCPY_TtoA(buf,payload,1024);
                        el.payload = buf;
                      }
                    }

                    r.elements.push_back(el);

                    unique_lookup[k] = true;
                }
            }


        } else {

            for ( int j=q.offset;j<mlen && j<(q.offset+q.limit); j++ ) {

                lucene::document::Document *doc = &h->doc(j);
                const wchar_t *id   = doc->get(DOC_KEY);

                if(id == NULL) {
                    T_ERROR("Dockey missing from document!")
                    continue;
                } else {
                    STRCPY_TtoA(buf,id,1024);

                    T_DEBUG("ID: %s",buf);

                    thrudex::Element el;
                    el.index = q.index;
                    el.key   = buf;

                    if(q.payload){
                        T_DEBUG("Fetching payload");
                        const wchar_t *payload = doc->get(DOC_PAYLOAD);
                        if(payload != NULL){
                            STRCPY_TtoA(buf,payload,1024);
                            el.payload = string(buf);
                        }
                    }

                    r.elements.push_back(el);
                }
            }
        }
    }


    _CLDELETE(h);
    _CLDELETE(lsort);
    _CLDELETE(query);

}

void CLuceneIndex::run()
{

    while(1){
        sleep(10);
        T_DEBUG("Syncing");
        sync();
        T_DEBUG("Syncing Finished");
    }

}

void CLuceneIndex::sync(bool force)
{
    //Any updates

    if(!force){
        Guard g(mutex);
        if(last_modified <= last_synched && disk_deletes->empty())
            return;
    }

    T_DEBUG("Syncing Started");
    string idx_path = index_root + "/" + index_name;

    shared_ptr<IndexModifier>       l_ram_modifier;
    shared_ptr<bloom_filter>        l_ram_bloom;
    shared_ptr<CLuceneRAMDirectory> l_ram_directory;
    shared_ptr<CLuceneRAMDirectory> l_ram_ro_dir;
    shared_ptr<set<string> >        l_disk_deletes;
    shared_ptr<bloom_filter>        l_disk_bloom;
    shared_ptr<UpdateFilter>        l_update_filter;


    //replace index handles
    {
        //RWGuard g(mutex,true);
        Guard g(mutex);

        syncing = true; //this flag alters the search code to include prev searcher

        //Flush old writer
        modifier->flush();

        //Grab old handles
        l_ram_bloom    = ram_bloom;
        l_ram_directory= ram_directory;
        l_disk_deletes = disk_deletes;
        l_disk_bloom   = disk_bloom;

        l_ram_ro_dir.reset( new CLuceneRAMDirectory( l_ram_directory.get() ) );
        l_ram_ro_dir->__cl_addref(); //trick clucene's lame ref counters

        //create new handles
        ram_directory.reset(new CLuceneRAMDirectory());
        ram_directory->__cl_addref(); //trick clucene's lame ref counters

        ram_bloom.reset(new bloom_filter(filter_space,1.0/(1.0 * filter_space), random_seed));
        modifier.reset(new IndexModifier(ram_directory.get(),analyzer.get(),true));

        ram_prev_prev_directory = ram_prev_directory;
        ram_prev_directory      = l_ram_directory;

        disk_deletes.reset(new set<string>());
    }

    T_DEBUG("Created Handles");

    {
        Guard g(mutex);
        //Now we start by deleting any updated docs from disk
        shared_ptr<IndexReader> tmp_disk_reader(IndexReader::open(idx_path.c_str()));

        int i=0;
        set<string>::iterator it;
        for( it=l_disk_deletes->begin(); it!=l_disk_deletes->end(); ++it){

            wstring wkey = build_wstring(*it);
            Term      *t = new Term(DOC_KEY, wkey.c_str() );

            tmp_disk_reader->deleteDocuments(t);

            T_DEBUG("Deleted %s",(*it).c_str());

            delete t;
            i++;
        }

        tmp_disk_reader->close();

        T_DEBUG("Deleted old ids");
    }


    {
        //Now merge in the ram
        shared_ptr<IndexWriter> disk_writer(new IndexWriter(idx_path.c_str(),analyzer.get(),false,false));
        disk_writer->setUseCompoundFile(true);

        Directory *dirs[2];

        dirs[0] = l_ram_ro_dir.get();
        dirs[1] = NULL;

        disk_writer->addIndexes(dirs);
        disk_writer->close();

        //Merge in the ram bloom
        *l_disk_bloom.get() |= *l_ram_bloom.get();


        T_DEBUG("Merged");
    }

    //Search new index (big perf hit so get it over now)
    shared_ptr<IndexReader>   l_disk_reader( IndexReader::open(idx_path.c_str()), reader_deleter() );
    shared_ptr<IndexSearcher> l_disk_searcher( new IndexSearcher(l_disk_reader.get()) );

    wstring q = wstring(DOC_KEY)+wstring(L":1234");

    Query *query = QueryParser::parse( q.c_str(),DOC_KEY,analyzer.get());
    Hits  *h     = l_disk_searcher->search(query);
    _CLDELETE(h);
    _CLDELETE(query);

    T_DEBUG("Query");

    //replace index handles
    {
        Guard g(mutex);

        //the order of these things really matters
        disk_searcher = l_disk_searcher;
        disk_filter.reset( new UpdateFilter(l_disk_reader) );
        disk_reader   = l_disk_reader;


        //Add any new deletes to the filter
        set<string>::iterator it;
        for( it=disk_deletes->begin(); it!=disk_deletes->end(); ++it){
            wstring wkey = build_wstring(*it);

            T_DEBUG("Skipping sync:%s",(*it).c_str());
            disk_filter->skip(wkey);
        }


        last_synched = Util::currentTime();

        syncing = false; //this flag alters the search code to include prev searcher
    }

    T_DEBUG("Set new search");
}


void CLuceneIndex::optimize()
{
    Guard g(mutex);

    T_DEBUG("Start Optimizing");

    string idx_path = index_root + "/" + index_name;
    shared_ptr<IndexWriter> disk_writer(new IndexWriter(idx_path.c_str(),analyzer.get(),false,false));
    disk_writer->setUseCompoundFile(true);

    disk_writer->optimize();

    T_DEBUG("Stop Optimizing");
}
