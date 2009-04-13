#ifndef __SHARED_MULTI_SEARCHER__
#define __SHARED_MULTI_SEARCHER__

#include <boost/shared_ptr.hpp>

#include <CLucene.h>
#include <CLucene/search/IndexSearcher.h>
#include <CLucene/search/MultiSearcher.h>

#include "CLuceneRAMDirectory.h"

/**
 *Manages a multi searcher by holding references to the underlying storage.
 *This is required to ensure the reference counters stay in scope and arent
 *lost when another thread changes the data
 **/
class SharedMultiSearcher
{
 public:
    SharedMultiSearcher(boost::shared_ptr<lucene::store::FSDirectory> disk_directory,
                        boost::shared_ptr<lucene::index::IndexReader> disk_reader,
                        boost::shared_ptr<lucene::search::IndexSearcher> disk_searcher,
                        boost::shared_ptr<lucene::store::CLuceneRAMDirectory> ram_directory,
                        boost::shared_ptr<lucene::store::CLuceneRAMDirectory> prev_ram_directory = boost::shared_ptr<lucene::store::CLuceneRAMDirectory>());

    ~SharedMultiSearcher();

    lucene::search::Hits *search(lucene::search::Query *query);
    lucene::search::Hits *search(lucene::search::Query *query, lucene::search::Filter *filter);
    lucene::search::Hits *search(lucene::search::Query *query, lucene::search::Filter *filter, lucene::search::Sort *sort);

 private:
    boost::shared_ptr<lucene::search::MultiSearcher>      multi_searcher;
    lucene::search::Searchable                            *searchables[4];

    boost::shared_ptr<lucene::store::FSDirectory>         disk_directory;
    boost::shared_ptr<lucene::index::IndexReader>         disk_reader;
    boost::shared_ptr<lucene::search::IndexSearcher>      disk_searcher;
    boost::shared_ptr<lucene::store::CLuceneRAMDirectory> ram_directory;
    boost::shared_ptr<lucene::index::IndexReader>         ram_reader;
    boost::shared_ptr<lucene::store::CLuceneRAMDirectory> prev_ram_directory;
    boost::shared_ptr<lucene::index::IndexReader>         prev_ram_reader;
    boost::shared_ptr<lucene::search::IndexSearcher>      ram_searcher;
    boost::shared_ptr<lucene::search::IndexSearcher>      prev_ram_searcher;

};

#endif
