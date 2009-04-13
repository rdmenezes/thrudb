#ifndef __CLUCENE_INDEX_H__
#define __CLUCENE_INDEX_H__

#include <boost/shared_ptr.hpp>

#include <concurrency/Mutex.h>
#include <concurrency/Monitor.h>
#include <concurrency/ThreadManager.h>
#include <concurrency/Util.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <map>
#include <vector>

#include <CLucene.h>
#include <CLucene/index/IndexModifier.h>
#include <CLucene/search/MultiSearcher.h>

#include "Thrudex.h"
#include "CLuceneRAMDirectory.h"
#include "SharedMultiSearcher.h"

class bloom_filter;
class UpdateFilter;

#define DOC_KEY L"_doc_key_"
#define DOC_PAYLOAD L"_payload_"

/***
 *Manages index reads and writes for optimal performance.
 *
 *The single biggest issue with Lucene is the performance hit you take
 *when incrementally updating an index.  Lucene works best with bulk writes
 *and infrequent index reader refreshes. However, this isn't the way we'd like to use it.
 *
 *This class addresses this issue by maintaining an in memory index for incremental
 *writes and a monitor thread that syncs them to disk once a memory or time limit is reached.
 *This way writes are instantly available to readers with little perf hit. sweet.
 *
 *Redo logging is employed elsewhere so we can recover if the system crashes before a sync has occurred.
 **/
class CLuceneIndex : public apache::thrift::concurrency::Runnable
{
 public:
    CLuceneIndex(const std::string &index_root,
                 const std::string &index_name,
                 const std::size_t &filter_space,
                 boost::shared_ptr<lucene::analysis::Analyzer> analyzer);

    ~CLuceneIndex();

    void put(const std::string &key, lucene::document::Document *doc );
    void remove(const std::string &key);
    void search(const thrudex::SearchQuery &s, thrudex::SearchResponse &r);

    void run();

    void optimize();

 private:
    void sync(bool force = false);

    boost::shared_ptr<SharedMultiSearcher>         getSearcher();
    boost::shared_ptr<apache::thrift::concurrency::Thread> monitor_thread;

    apache::thrift::concurrency::Mutex             mutex;

    const std::string                                index_root;
    const std::string                                index_name;
    boost::shared_ptr<lucene::analysis::Analyzer>    analyzer;

    std::size_t filter_space;

    boost::shared_ptr<lucene::index::IndexModifier>  modifier;
    volatile int64_t                                 last_modified;

    boost::shared_ptr<SharedMultiSearcher>           searcher;
    int64_t                                          last_refresh;

    int64_t                                          last_synched;
    volatile bool                                    syncing;

    boost::shared_ptr<lucene::store::FSDirectory>    disk_directory;
    boost::shared_ptr<lucene::index::IndexReader>    disk_reader;
    boost::shared_ptr<UpdateFilter>                  disk_filter;
    boost::shared_ptr<lucene::search::IndexSearcher> disk_searcher;
    boost::shared_ptr<bloom_filter>                  disk_bloom;
    boost::shared_ptr<std::set<std::string> >        disk_deletes;

    boost::shared_ptr<lucene::store::CLuceneRAMDirectory>  ram_directory;
    boost::shared_ptr<lucene::store::CLuceneRAMDirectory>  ram_prev_directory;

    boost::shared_ptr<lucene::store::CLuceneRAMDirectory>  ram_prev_prev_directory;



    boost::shared_ptr<bloom_filter>                  ram_bloom;

    std::size_t random_seed;
};

#endif
