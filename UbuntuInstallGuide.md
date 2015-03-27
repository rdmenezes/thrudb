### NOTE: We've begin an alternate implementation in java on github: http://github.com/tjake/thrudb/tree/master ###


**1. Initial Setup**

Most of the dependencies are available with apt-get, but some we’ll have to build from source. The first thing we need for thrudb is thrift, which itself has  a few dependencies.

First install our build tools and thrift dependencies:

```
$ sudo apt-get -y install subversion g++ make flex bison python-dev libboost-dev libevent-dev automake pkg-config libtool make
```



---

**2. Thrift**

Now let’s grab thrift from SVN:

```
$ svn co http://svn.apache.org/repos/asf/incubator/thrift/trunk thrift
$ cd thrift
```

**2.1 Optional Java Support**

In order to configure Thrift with java support you will need Java installed first:

```
$ sudo apt-get -y install sun-java5-jdk ant
```

This will take another 5 minutes or so. When prompted with the Java license, hit 

&lt;tab&gt;



&lt;enter&gt;

 twice.

Then build and install Thrift (don’t worry about the bootstrap.sh warnings):

```
$ cd thrift
$ ./bootstrap.sh
$ ./configure
$ make
$ sudo make install
```

Again, this will take a few minutes.



---

**3. Thrift Client Libraries**

Now that thrift is installed, we need to install the client libraries for whichever language(s) that we’re planning on using. The C++ and Python libraries are installed by default, but this guide will focus on Java, Ruby, Perl as examples. If you get other client libraries working, leave a comment with the steps taken and I’ll amend this post.

To build the Java client libraries:

```
$ cd lib/java
$ sudo ant install
$ cd ../..
```

This will install the thrift JAR file to /usr/local/lib/libthrift.jar. For the Perl client libraries, we have another dependency to install:

```
$ sudo perl -MCPAN -e "install Bit::Vector"
```

Enter yes at the first prompt, then accept the defaults for the dozen or so prompts that follow. Now we can build and install:

```
$ cd lib/perl
$ perl Makefile.PL
$ make
$ sudo make install
$ cd ../..
```

For the '''Ruby client libraries''':
```
$ cd lib/rb/
$ sudo ruby setup.rb config
$ sudo ruby setup.rb install
```

For the PHP5 client libraries:
```
$ cd lib/php
$ sudo mkdir -p /usr/lib/php5/thrift
$ sudo cp -pvr src/* /usr/lib/php5/thrift/
```

Please take a look at '''lib/csharp|st|erl>/README''' if you want to install other client libraries. Alright, done with thrift.


---

**4. Thrudb Dependencies**

Let’s go back to our build dir, and start on the other dependencies for thrudb: **NOTE: many of these are optional**

```
#Required packages
$ sudo apt-get install uuid-dev libssl-dev

#Optional packages
$ sudo apt-get -y install memcached libexpat1-dev libcurl4-openssl-dev libmysql++-dev libdb4.5++-dev libltdl3-dev
```

These will take a few minutes each. Accept all defaults when prompted.

There are 3 dependencies that we’ll install from source: libmemcached, Spread, and CLucene (since, as of this writing, apt-get has Spread 3.x and we need Spread 4.x; the same with CLucene, apt-get has 0.18.x and we need 0.19.x ).

**libmemcached (OPTIONAL)**

Now we’ll get, build, and install libmemcached:

```
$ sudo apt-get install curl
$ curl http://download.tangent.org/libmemcached-0.25.tar.gz | tar xzf -
$ cd libmemcached-0.25
$ ./configure
$ make
$ sudo make install
$ cd ..
```

**Spread (OPTIONAL)**

Spread requires filling in a form to download it, which will be tricky from the command line, but curl to the rescue (replace the values of name, company, and email in the first command with your own information):

```
$ curl -L -d FILE=spread-src-4.0.0.tar.gz -d name="Thrudb User" -d company="Thrudb User" -d email="unknown@thrudb.org" -d Stage=Download http://www.spread.org/download/spread-src-4.0.0.tar.gz | tar xzf -
$ cd spread-src-4.0.0
$ ./configure
$ make
$ sudo make install
$ cd ..
```

**CLucene**

Depending on your version of ubuntu you may have the correct version available. To check run:

```
$ apt-cache show libclucene-dev 
```

If the version number is 0.19.x or greater then u can install `$sudo apt-get install libclucene-dev`

Otherwise you'll need to download, build, and install the latest version of CLucene:


```
$ curl http://internap.dl.sourceforge.net/sourceforge/clucene/clucene-core-0.9.21b.tar.gz | tar zxf -
$  cd clucene-core-0.9.21b/
$ ./configure
$ make
$ sudo make install
```


---

**5. Install Thrudb**

Well, if you’ve made it this far, congratulate yourself. We’re now ready to actually install thrudb! First we’ll get it from SVN:

```
$ svn co http://thrudb.googlecode.com/svn/trunk/ thrudb
$ cd thrudb
```

To build and install everything:

```
$ ./bootstrap.sh
$ ./configure 
$ make
$ sudo make install
```

You can alse build specific portions, thrucommon is required, everything after is if you want it:

```
$ cd thrucommon
$ ./bootstrap.sh
$ ./configure
$ make
$ sudo make install
```

Repeat for each of the pieces you want, i.e. thrudoc, thrudex, thruqueue, throxy (throxy is not ready yet though).  Some of them may require 'sudo ./bootstrap.sh' if you get some permission errors.


---

**6. Thrudb Client Libraries and Tutorials**

And finally, we just need to build the thrudb client libraries (similar to what we did for thrift), and tutorials if we want to test it out.

To build the client libraries, you just need to run /usr/local/bin/thrift on Thrudoc.thrift and Thrucene.thrift and specify which language to generate. However, there’s a handy Makefile in the tutorial directory that will take care of this for us:

```
$ cd tutorial
$ make
```

Start thrudoc and thrucene with the handy control script:

```
$ ./thrudbctl start
```

At last!

All the tutorials should work out of the box. Let’s run the Python tutorial:

```
$ cd py
$ python BookmarkExample.py
```

You should see something like:

```
*Indexed file in: 0.20 seconds*

Searching for: tags:(+css +examples)
Found 3 bookmarks
1       title:  Dynamic Drive CSS Library- Practical CSS codes and examples
        url:    (http://www.dynamicdrive.com/style/)
        tags:   (css examples)
2       title:  Dynamic Drive DHTML Scripts -DD Tab Menu (5 styles)
        url:    (http://www.dynamicdrive.com/dynamicindex1/ddtabmenu.htm)
        tags:   (cool css examples menu)
3       title:  Uni-Form
        url:    (http://dnevnikeklektika.com/uni-form/)
        tags:   (examples CSS)
Took: 0.00 seconds

Searching for: title:(linux)
Found 4 bookmarks
1       title:  Debian GNU/Linux System Administration Resources
        url:    (http://www.debian-administration.org/)
        tags:   (linux administration tips)
2       title:  Linux Scalability
        url:    (http://www.cs.wisc.edu/condor/condorg/linux_scalability.html)
        tags:   (linux sysadmin ulimit)
3       title:  Set Up Postfix For Relaying Emails Through Another Mailserver | HowtoForge - Linux Howtos and Tutorials
        url:    (http://www.howtoforge.com/postfix_relaying_through_another_mailserver)
        tags:   (email linux server)
4       title:  ZFS on FUSE/Linux
        url:    (http://zfs-on-fuse.blogspot.com/)
        tags:   (zfs linux fuse)
Took: 0.00 seconds

*Index cleared in: 0.06 seconds*
```

The Perl tutorial needs one more dependency:

```
$ sudo perl -MCPAN -e "install Class::Accessor"
```

Again, accept the default when prompted. Then run with:

```
$ cd perl
$ perl BookmarkExample.pl
```

You should see output similar to the Python example.



---

**7. Next Steps**

Congratulations! You now have thrudoc and thrucene running on your own Ubuntu box. From here, you can poke around in the tutorial directory and have a look at the 