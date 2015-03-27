# ACHTUNG! ThruDB is no longer under development #

Thrudb is a set of simple services built on top of the ~~Facebook~~ [Apache Thrift framework](http://incubator.apache.org/thrift/) that provides indexing and document
storage services for building and scaling websites. Its purpose is to offer web developers flexible, fast and
easy-to-use services that can enhance or replace traditional data storage and access layers.

## Thrudb - High Level Features ##

  * Client libraries for most languages
  * Incremental backups and redo logging
  * Multiple storage backends (BerkeleyDB, Disk, MySQL, S3 included)
  * Memcache and Spread integration.
  * Built for horizontal scalability
  * Simple and powerful search service

## Thrudb - Services ##

  * Thrudoc - Document storage service
  * Thrudex - Indexing and search service
  * Thruqueue - Persistent message queue service ([read more](http://3.rdrail.net/blog/announcing-thruqueue-persistant-message-queue-for-thrudb))
  * Throxy - Scaling service

## Thrudb - Documentation ##

> A good place to start is our [Technical Document](http://thrudb.googlecode.com/svn/trunk/doc/Thrudb.pdf)

> The wiki contains installation and configuration info.  Please ask questions in our discussion group.




We haven't quite made it to a "release" yet so for now go ahead and grab the code with svn
### We've begin an alternate implementation in java on github: http://github.com/tjake/thrudb/tree/master ###

See Thrudb in action on http://www.junkdepot.com, which is built with Thrudb as its data backend (no RDBMS) and is running on Amazon EC2 with virtually no chance of data loss (thanks to S3 storage backend).