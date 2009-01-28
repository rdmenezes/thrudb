# TODO: figure out the right way to link with libthrudoc for apps, making sure
# that we're linked to the one we're building and not any installed verison...

pkgconfigdir = $(libdir)/pkgconfig

bin_PROGRAMS = thrudoc thrudoc_replay
noinst_PROGRAMS = thrudoc_bench
lib_LTLIBRARIES = libthrudoc.la
pkgconfig_DATA = thrudoc.pc

include_thrudocdir = $(includedir)/thrudoc
include_thrudoc_HEADERS = \
			     app_helpers.h		\
			     BDBBackend.h		\
			     BloomBackend.h		\
			     DiskBackend.h		\
			     LogBackend.h		\
			     MemcachedBackend.h		\
			     MySQLBackend.h		\
			     NBackend.h			\
			     NullBackend.h		\
			     ReplicationBackend.h	\
			     S3Backend.h		\
			     SpreadBackend.h		\
			     StatsBackend.h		\
			     ThrudocBackend.h

libthrudoc_la_SOURCES = \
		  gen-cpp/Thrudoc.cpp			\
		  gen-cpp/Thrudoc_types.cpp		\
		  gen-cpp/Thrudoc_constants.cpp		\
		  app_helpers.cpp			\
		  ThrudocBackend.cpp			\
		  ThrudocHandler.cpp			\
		  LogBackend.cpp			\
		  BloomBackend.cpp			\
		  BDBBackend.cpp			\
		  DiskBackend.cpp			\
		  MySQLBackend.cpp			\
		  MemcachedBackend.cpp			\
		  NBackend.cpp				\
		  NullBackend.cpp			\
		  ReplicationBackend.cpp		\
		  S3Backend.cpp				\
		  StatsBackend.cpp			\
		  SpreadBackend.cpp			\
		  base64.cpp				\
		  s3_glue.cpp				\
		  mysql_glue.cpp

libthrudoc_la_CPPFLAGS = -Wall -Igen-cpp $(CURL_CFLAGS) $(MEMCACHED_CFLAGS) $(THRIFTNB_CFLAGS) -I../../thrucommon/src -I../../thrucommon/src/gen-cpp $(SSL_CFLAGS) $(UUID_CFLAGS) $(MYSQL_CFLAGS) $(BERKELEY_DB_CPPFLAGS) $(BOOST_CPPFLAGS) $(SPREAD_CFLAGS)
libthrudoc_la_LDFLAGS = -Wall $(CURL_LIBS) $(MEMCACHED_LIBS) $(THRIFTNB_LIBS) $(SSL_LIBS) $(UUID_LIBS) $(MYSQL_LDFLAGS) $(BERKELEY_DB_LDFLAGS) $(BERKELEY_DB_LIB) $(LIBEVENT_LDFLAGS) $(LIBEVENT_LIBS) $(BOOST_LDFLAGS)  $(BOOST_FILESYSTEM_LIB) $(BOOST_SYSTEM_LIB) $(SPREAD_LIBS)

LDADD = $(top_builddir)/src/libthrudoc.la ../../thrucommon/src/libthrucommon.la

thrudoc_CPPFLAGS = -Wall -Igen-cpp $(CURL_CFLAGS) $(MEMCACHED_CFLAGS) $(THRIFTNB_CFLAGS) -I../../thrucommon/src -I../../thrucommon/src/gen-cpp $(SSL_CFLAGS) $(UUID_CFLAGS)
thrudoc_LDADD = $(LDADD)
thrudoc_LDFLAGS = -Wall -rpath $(pkglibdir) $(CURL_LIBS) $(MEMCACHED_LIBS) $(THRIFTNB_LIBS) $(SSL_LIBS) $(UUID_LIBS) $(BERKELEY_DB_LDFLAGS) $(BERKELEY_DB_LIB) 
thrudoc_SOURCES = \
		  thrudoc.cpp

thrudoc_replay_CPPFLAGS = -Wall -Igen-cpp $(CURL_CFLAGS) $(MEMCACHED_CFLAGS) $(THRIFTNB_CFLAGS) -I../../thrucommon/src -I../../thrucommon/src/gen-cpp $(SSL_CFLAGS) $(UUID_CFLAGS)
thrudoc_replay_LDADD = $(LDADD) 
thrudoc_replay_LDFLAGS = -Wall -rpath $(pkglibdir) $(CURL_LIBS) $(MEMCACHED_LIBS) $(THRIFTNB_LIBS) $(SSL_LIBS) $(UUID_LIBS) 
thrudoc_replay_SOURCES = \
		  Replayer.cpp \
		  thrudoc_replay.cpp



thrudoc_bench_SOURCES = \
			thrudoc_bench.cpp
thrudoc_bench_LDADD = $(LDADD)
thrudoc_bench_CPPFLAGS = -Wall -Igen-cpp $(CURL_CFLAGS) $(MEMCACHED_CFLAGS) $(THRIFTNB_CFLAGS) -I../../thrucommon/src -I../../thrucommon/src/gen-cpp $(SSL_CFLAGS) $(UUID_CFLAGS) 
thrudoc_bench_LDFLAGS = -Wall $(THRIFT_LDFLAGS) $(THRIFT_LIBS)

gen-cpp/Thrudoc.cpp: Thrudoc.thrift
	${THRIFT} --gen cpp $<

clean-local:
	rm -fr gen-cpp
