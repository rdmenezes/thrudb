[Lucene](http://lucene.apache.org) is an apache library that provides a powerful api for search.
It powers search on a number of sites like Wikipedia and Technorati.
[CLucene](http://clucene.sf.net) is a full c++ port of lucene that boasts 10x performance increase over java lucene.

One of the issues with Lucene is the cost of index updates.  Most services only do batch updates because the cost of re-opening the index is so high.

To address this we've designed a hybrid approach that allows writes to always happen on an in memory index.
Every 30 seconds a background thread syncs the latest writes down to a disk index and reopens the index in the background.
This allows thrudex to use the lucene as a live index service.

When a search occurs, both the in-memory and disk indexes are searched in parallel making writes instantly available.

If a write is an update of a document on disk already then a disk index search filter in updated so we ignore old documents.

''>1000 TPS mixed read/write''