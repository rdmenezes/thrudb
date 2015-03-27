The Memcache backend provides a caching layer on top of a persistent backend. It operates transparently intercepting calls to the primary backend and sets and gets data from a Memcached pool of servers lowering the load on the primary and increasing performance.

The memcached caching backend is powered by libmemcached a full featured C memcached client library.
Features

  * Transparent and simple caching of any persistent backend
  * Single configuration value setup.

Future Direction & Development

  * Expose as much of the underlying libmemcached features as possible/makes sense

Use & Configuration
To use the memcached caching backend add the following line to your thrudoc.conf replacing the server names and ports with the list of hosts in your memcached pool.

```
    MEMCACHED_SERVERS = <server.one:port,server.two:port,...>
```