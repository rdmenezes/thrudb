pkgconfigdir = $(libdir)/pkgconfig

bin_PROGRAMS = thrudex thrudex_bench thrudex_replay
noinst_PROGRAMS = thrudex_bench
lib_LTLIBRARIES = libthrudex.la
pkgconfig_DATA = thrudex.pc


include_thrudexdir = $(includedir)/thrudex

include_thrudex_HEADERS = \
		  gen-cpp/Thrudex.h			\
		  gen-cpp/Thrudex_types.h		\
		  gen-cpp/Thrudex_constants.h		\
		  app_helpers.h 			\
		  ThrudexHandler.h			\
		  LogBackend.h   			\
		  ThrudexBackend.h			\
		  CLuceneBackend.h			\
		  CLuceneRAMDirectory.h                 \
		  CLuceneIndex.h			\
		  StatsBackend.h 			\
		  SharedMultiSearcher.h			\
		  UpdateFilter.h

libthrudex_la_SOURCES = \
		  gen-cpp/Thrudex.cpp			\
		  gen-cpp/Thrudex_types.cpp		\
		  gen-cpp/Thrudex_constants.cpp		\
		  app_helpers.cpp			\
		  ThrudexHandler.cpp			\
		  LogBackend.cpp			\
		  ThrudexBackend.cpp			\
		  CLuceneBackend.cpp			\
		  CLuceneRAMDirectory.cpp               \
		  CLuceneIndex.cpp			\
		  StatsBackend.cpp			\
		  SharedMultiSearcher.cpp		\
		  UpdateFilter.cpp

libthrudex_la_CPPFLAGS=-Wall -Igen-cpp $(THRIFTNB_CFLAGS) -I../../thrucommon/src -I../../thrucommon/src/gen-cpp  $(UUID_CFLAGS) $(BOOST_CPPFLAGS) $(CLUCENE_CPPFLAGS)
libthrudex_la_LDFLAGS = -Wall $(THRIFTNB_LIBS) $(UUID_LIBS) $(BOOST_LDFLAGS) $(BOOST_FILESYSTEM_LIB) $(CLUCENE_LDFLAGS) $(LIBEVENT_LDFLAGS) $(LIBEVENT_LIBS) $(BOOST_SYSTEM_LIB)

LDADD = $(top_builddir)/src/libthrudex.la ../../thrucommon/src/libthrucommon.la

thrudex_SOURCES = \
		  thrudex.cpp
thrudex_CPPFLAGS = -Wall -Igen-cpp $(THRIFTNB_CFLAGS) -I../../thrucommon/src -I../../thrucommon/src/gen-cpp  $(UUID_CFLAGS) $(BOOST_CPPFLAGS) $(CLUCENE_CPPFLAGS)
thrudex_LDFLAGS = -Wall $(THRIFTNB_LIBS) $(UUID_LIBS) $(BOOST_LDFLAGS) $(BOOST_FILESYSTEM_LIB) $(CLUCENE_LDFLAGS) $(BOOST_SYSTEM_LIB)
thrudex_LDADD = $(LDADD)


thrudex_replay_CPPFLAGS = -Wall -Igen-cpp $(CURL_CFLAGS) $(MEMCACHED_CFLAGS) $(THRIFTNB_CFLAGS) -I../../thrucommon/src -I../../thrucommon/src/gen-cpp $(SSL_CFLAGS) $(UUID_CFLAGS)  $(BOOST_CPPFLAGS)
thrudex_replay_LDADD = $(LDADD)
thrudex_replay_LDFLAGS = -Wall -rpath $(pkglibdir) $(THRIFTNB_LIBS) $(SSL_LIBS) $(UUID_LIBS) $(BOOST_LDFLAGS) $(BOOST_FILESYSTEM_LIB) 
thrudex_replay_SOURCES = \
		  thrudex_replay.cpp

thrudex_bench_SOURCES = \
			thrudex_bench.cpp

thrudex_bench_CPPFLAGS = -Wall -Igen-cpp $(THRIFTNB_CFLAGS) -I../../thrucommon/src -I../../thrucommon/src/gen-cpp $(BOOST_CPPFLAGS)
thrudex_bench_LDFLAGS = -Wall $(THRIFTNB_LDFLAGS) $(THRIFTNB_LIBS) -lthrudex

gen-cpp/Thrudex.cpp: Thrudex.thrift
	${THRIFT} -cpp $<

clean-local:
	rm -fr gen-cpp
