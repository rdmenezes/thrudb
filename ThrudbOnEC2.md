**1. AMI information**

If you are on Amazon EC2, you may want to check out these Thrudb AMIs (the OS is Ubuntu 7.10):

+ i386: ami-c71efbae

+ x86\_64: ami-dc1efbb5

Note: guys at aideRSS had released some [Thrudb AMIs](http://blog.aiderss.com/2007/12/18/announcing-thrudb-ec2-public-amis/) but they used the old Thrudb source though. My AMIs use the latest version of Thrudb.

**2. Start your instance**

Please consult [Amazon EC2 documentation](http://developer.amazonwebservices.com/connect/entry.jspa?externalID=992&categoryID=87) for how to start your instance. BTW, I highly recommend [EC2 Firefox UI](http://s3.amazonaws.com/ec2-downloads/ec2ui.xpi) which is very easy to use.

**3. Start Thrudb**

Once you login into your EC2 instance, run these commands to start thrudoc and thrudex:

```
# cd /root/buildthrudb/thrudb/tutorial
# make start
```

Just ignore any output or warning. Use nestat to verify if thrudoc and thrudex has been started:

```
# netstat -npaut
```

You should see lt-thrudoc and lt-thrudex are listening on 0.0.0.0:11291 and 0.0.0.0:11299 respectively.

**4. Run tutorials**

All the tutorials should work out of the box. Let’s run the Python tutorial:

```
# cd py/
# python BookmarkExample.py
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

**5. Update Thrift/Thrudb**

Thrift and Thrudb is in very active development so you may want to update their sources to run the latest versions. To update Thrift, you run these commands:

```
# cd /root/buildthrudb/thrift/
# svn update
# ./bootstrap.sh
# ./configure
# make
# make install
```

And you update Thrudb with these commands:

```
# cd /root/buildthrudb/thrudb/
# svn update
# make
```

**6. Next steps**

Congratulations! You now have thrudoc and thrudex running on your own EC2 instance. From here, you can poke around in the tutorial directory and have a look at the **.conf files. You may also want to join [discussion list](http://groups.google.com/group/thrudb).**

Remember to terminate your instance if you're done playing with Thrudb.