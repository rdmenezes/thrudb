The MySQL backend provides an exteremly fast and scalable persistent storage backend.
Features

  * Massively Scalable
    * >4000 TPS mixed read/write with a single modest desktop box running both the MySQL and Thrudoc servers
    * Partitioned data storage to allow near-infinite dataset size
    * Simple setup, just install MySQL and Thrudoc, run the tablename create script, and set a couple of configuration values
  * Supports automatic and instant read-only failover and master switchback.
  * Straightforward in DB storage, so you can easily write your own apps/scripts to work with the data
  * Works with any MySQL setup: replication, backup, ...

Suggested/Proposed Architecture

[[Image(mysql.png)]].

One or more Thrudoc servers fronting a pool of two or more MySQL servers in master-master pairs. datatables are assigned to each mysql server and it's sibling will provide read-only failover. Once the downed host returns to service the Thrudoc servers will automatically switch back to it in read-write mode without any data loss. If for some reason the host can not be brought back up a manual read-write failover can take place, to the sibling if it can handle the load of both, or to a host replacing the downed one. At most replication delay information is lost, if the downed host can't be brought back up.
Future Direction & Development

  * Directory Manager
    * Split and combine tables to grow/shrink the backend and balance load
  * Multi-master (ring replication) fail-over support

Use & Configuration

```
    mysql_backend_create_tablename.pl --master-hostname=<host> --master-port=<port> --database=<db>
        --root-pass=<password> --tablename=<tablename> --hosts=<host1,host2,...> --parts-per=<2> 
        --with-circular-slaves
```

Sensible defaults are provided for all options above. If --with-circular-replication is provided there must be an even number of hosts and they will be paired up in the order given.
To use the MySQL backend add the following lines to your thrudoc.conf replacing the right hand sides with values appropriate for your environment.

```
    MYSQL_MASTER_HOST = <host>
    MYSQL_MASTER_PORT = <port>
    MYSQL_SLAVE_HOST = <host>
    MYSQL_SLAVE_PORT = <port>
    MYSQL_DIRECTORY_DB = <db>
    MYSQL_USERNAME = <username>
    MYSQL_PASSWORD = <password>
```