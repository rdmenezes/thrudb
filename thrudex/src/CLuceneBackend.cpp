#ifdef HAVE_CONFIG_H
#include "thrudex_config.h"
#endif
/* hack to work around thrift and log4cxx installing config.h's */
#undef HAVE_CONFIG_H

#include "CLuceneBackend.h"
#include "ConfigFile.h"
#include "utils.h"
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <concurrency/Util.h>

namespace fs = boost::filesystem;
using namespace boost;
using namespace facebook::thrift::concurrency;

using namespace thrudex;

CLuceneBackend::CLuceneBackend(const string &idx_root)
    : idx_root(idx_root)
{
    T_DEBUG( "CLuceneBackend: idx_root=%s", idx_root.c_str() );
    //Verify log dir
    if(!directory_exists( idx_root )){
        fs::create_directories( idx_root );
    }

    analyzer = boost::shared_ptr<lucene::analysis::Analyzer>(new lucene::analysis::standard::StandardAnalyzer());

    //grab the list of current indices
    boost::filesystem::directory_iterator end;

    for(boost::filesystem::directory_iterator i(idx_root); i != end; ++i){

        if(is_directory (i->status ()))
        {
            //skip hidden dirs (.)
            if(i->path().leaf().substr(0,1) == ".")
                continue;

            this->addIndex( i->path().leaf() );

            T_DEBUG( "Added %s",i->path().leaf().c_str() );
        }
    }
}

CLuceneBackend::~CLuceneBackend()
{

}

bool CLuceneBackend::isValidIndex(const string &index)
{
    RWGuard g(mutex);

    return (index_cache.count(index) ? true : false);
}


vector<string> CLuceneBackend::getIndices()
{
    T_DEBUG( "getIndices:" );

    RWGuard g(mutex);

    vector<string> indices;

    map<string,shared_ptr<CLuceneIndex> >::iterator it;

    for(it = index_cache.begin(); it != index_cache.end(); ++it){
        indices.push_back(it->first);
    }

    return indices;
}

void CLuceneBackend::addIndex(const string &index)
{
    T_DEBUG( "addIndex: index=%s", index.c_str() );

    //Is index already loaded?
    if(this->isValidIndex(index))
        return;

    size_t filter_space = ConfigManager->read<int>("FILTER_SPACE_SIZE",1000000);

    index_cache[index] =
        shared_ptr<CLuceneIndex>(new CLuceneIndex(idx_root,index,filter_space,analyzer));
}


void CLuceneBackend::put(const thrudex::Document &d)
{
    T_DEBUG( "put: d.index=%s, d.key=%s", d.index.c_str(), d.key.c_str() );

    if(!this->isValidIndex( d.index )){
        ThrudexException ex;
        ex.what = "Invalid index: "+d.index;

        throw ex;
    }

    lucene::document::Document *doc = new lucene::document::Document();

    try{

        wstring doc_key     = build_wstring( d.key );

        if( doc_key.empty() ){
            ThrudexException ex;
            ex.what = "Missing key";

            throw ex;
        }

        lucene::document::Field *f;

        //add the document key
        f = lucene::document::Field::Keyword( DOC_KEY, doc_key.c_str() );
        doc->add(*f);

        //check for payload
        if( !d.payload.empty() ){
          f = lucene::document::Field::UnIndexed( DOC_PAYLOAD, build_wstring(d.payload).c_str() );
          doc->add(*f);
        }

        //populate the document
        for( unsigned int j=0; j<d.fields.size(); j++){

            T_DEBUG("%s:%s",d.fields[j].key.c_str(),d.fields[j].value.c_str());

            wstring key   = build_wstring( d.fields[j].key   );
            wstring value = build_wstring( d.fields[j].value );


            switch(d.fields[j].type){
                case thrudex::KEYWORD:
                    T_DEBUG("Keyword");
                    f = lucene::document::Field::Keyword(key.c_str(),value.c_str());  break;
                case thrudex::TEXT:
                    T_DEBUG("Text");
                    f = lucene::document::Field::Text(key.c_str(),value.c_str());     break;
                default:
                    T_DEBUG("UnStored");
                    f = lucene::document::Field::UnStored(key.c_str(),value.c_str()); break;
            };

            if(d.fields[j].weight> 0){
                f->setBoost( d.fields[j].weight );
            }


            doc->add(*f);

            //If Sorted field then add _sort
            if(d.fields[j].sortable){
                key += L"_sort";

                f = lucene::document::Field::Keyword(key.c_str(),value.c_str());

                doc->add(*f);
            }
        }

        if(d.weight > 0)
            doc->setBoost(d.weight);


        index_cache[d.index]->put( d.key, doc );

    }catch(...){
        //
    }

    delete doc;
}

void CLuceneBackend::remove(const thrudex::Element &el)
{
    T_DEBUG( "remove: el.index=%s, el.key", el.index.c_str(), el.key.c_str() );

    if(!this->isValidIndex( el.index )){
        ThrudexException ex;
        ex.what = "Invalid index: "+el.index;

        throw ex;
    }

    index_cache[el.index]->remove(el.key);
}


void CLuceneBackend::search(const thrudex::SearchQuery &q, thrudex::SearchResponse &r)
{
    T_DEBUG( "search: q.index=%s, q.query=%s", q.index.c_str(), q.query.c_str() );

    if(!this->isValidIndex( q.index )){
        ThrudexException ex;
        ex.what = "Invalid index: "+q.index;

        throw ex;
    }

    index_cache[q.index]->search(q,r);
}


string CLuceneBackend::admin(const std::string &op, const std::string &data)
{
    T_DEBUG( "admin: op=%s, data=%s", op.c_str(), data.c_str() );

    string log_pos_file = idx_root + "/thrudex.state";

    if(op == "create_index"){

        string name = data;

        for(unsigned int i=0; i<name.size(); i++){
            if( !isascii(name[i]) ){
                ThrudexException ex;
                ex.what = "Index name contains non-ascii chars";
                throw ex;
            }


            if(name[i] == '\t' || name[i] == '|' || name[i] == '\n' || name[i] == '.' ||
               name[i] == '\\' || name[i] == '/' ){

                ThrudexException ex;
                ex.what = "Index name contains illegal chars";
                throw ex;
            }


        }

        T_DEBUG( "Creating index:%s",name.c_str());
        this->addIndex(name);

        return "ok";
    } else if (op == "put_log_position") {
        fs::ofstream outfile;
        outfile.open( log_pos_file.c_str (),
                      ios::out | ios::binary | ios::trunc);
        if (!outfile.is_open ())
        {
            ThrudexException e;
            e.what = "can't open log posotion file=" + log_pos_file;
            T_ERROR( e.what.c_str());
            throw e;
        }
        outfile.write (data.data (), data.size ());
        outfile.close();
        return "done";
    } else if (op == "get_log_position") {
        fs::ifstream infile;
        infile.open (log_pos_file.c_str (), ios::in | ios::binary | ios::ate);
        if (!infile.is_open ())
        {
            ThrudexException e;
            e.what = "can't open log posotion file=" + log_pos_file;
            T_ERROR( e.what.c_str());
            throw e;
        }
        fs::ifstream::pos_type size = infile.tellg ();
        char * memblock = new char [size];

        infile.seekg (0, ios::beg);
        infile.read (memblock, size);

        infile.close ();

        string obj (memblock, size);

        delete [] memblock;

        return obj;
    }

    return "";
}
