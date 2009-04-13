#ifdef HAVE_CONFIG_H
#include "thrudex_config.h"
#endif
/* hack to work around thrift and log4cxx installing config.h's */
#undef HAVE_CONFIG_H

#include "CLuceneIndex.h"
#include "utils.h"
#include <concurrency/Util.h>
#include <concurrency/PosixThreadFactory.h>

#include "ThruLogging.h"
#include "SharedMultiSearcher.h"

using namespace thrudex;


using namespace boost;
using namespace apache::thrift::concurrency;

using namespace lucene::index;
using namespace lucene::store;
using namespace lucene::search;
using namespace lucene::analysis;
using namespace lucene::util;
using namespace lucene::queryParser;
using namespace lucene::document;


SharedMultiSearcher::SharedMultiSearcher(boost::shared_ptr<lucene::store::FSDirectory> disk_directory,
                                         boost::shared_ptr<lucene::index::IndexReader> disk_reader,
                                         boost::shared_ptr<lucene::search::IndexSearcher> disk_searcher,
                                         boost::shared_ptr<lucene::store::CLuceneRAMDirectory> ram_directory,
                                         boost::shared_ptr<lucene::store::CLuceneRAMDirectory> prev_ram_directory)
    : disk_directory(disk_directory), disk_reader(disk_reader), disk_searcher(disk_searcher)
{

    //make a copy of the ram dir since its not thread safe
    this->ram_directory = shared_ptr<CLuceneRAMDirectory>( new CLuceneRAMDirectory( ram_directory.get() ));
    this->ram_directory->__cl_addref(); //trick clucene's lame ref counters

    this->ram_reader = shared_ptr<IndexReader>( IndexReader::open(this->ram_directory.get(), true));


    ram_searcher = shared_ptr<IndexSearcher>(new IndexSearcher( this->ram_reader.get() ));


    searchables[0] = this->ram_searcher.get();
    searchables[1] = this->disk_searcher.get();
    searchables[2] = NULL;
    searchables[3] = NULL;

    if(prev_ram_directory.get() != NULL){

        //make a copy of the ram dir since its not thread safe
        this->prev_ram_directory = shared_ptr<CLuceneRAMDirectory>( new CLuceneRAMDirectory( prev_ram_directory.get() ));
        this->prev_ram_directory->__cl_addref(); //trick clucene's lame ref counters

        this->prev_ram_reader = shared_ptr<IndexReader>( IndexReader::open(this->prev_ram_directory.get(), true));

        prev_ram_searcher = shared_ptr<IndexSearcher>(new IndexSearcher( this->prev_ram_reader.get() ));
        searchables[2] = this->prev_ram_searcher.get();
    }


    //allocate multi-searcher
    multi_searcher = shared_ptr<MultiSearcher>(new MultiSearcher(searchables));

}


SharedMultiSearcher::~SharedMultiSearcher()
{
    multi_searcher.reset();

    if(prev_ram_directory.get() != NULL){
        prev_ram_searcher.reset();
        prev_ram_reader.reset();
        prev_ram_directory.reset();
    }

    ram_searcher.reset();
    ram_reader.reset();
    ram_directory.reset();

    disk_searcher.reset();
    disk_reader.reset();
    disk_directory.reset();
}


Hits *SharedMultiSearcher::search(Query *q)
{
    return multi_searcher->search(q);
}

Hits *SharedMultiSearcher::search(Query *q, Filter *f)
{
    return multi_searcher->search(q,f);
}

Hits *SharedMultiSearcher::search(Query *q, Filter *f, Sort *s)
{
    return multi_searcher->search(q,f,s);
}
