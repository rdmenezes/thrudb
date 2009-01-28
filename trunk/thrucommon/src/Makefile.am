
pkgconfigdir = $(libdir)/pkgconfig

bin_PROGRAMS = thrurecorder
lib_LTLIBRARIES = libthrucommon.la
pkgconfig_DATA = thrucommon.pc

include_thrucommondir = $(includedir)/thrucommon
include_thrucommon_HEADERS = \
			     gen-cpp/Redo.h		\
			     gen-cpp/Redo_types.h	\
			     gen-cpp/EventLog.h		\
			     gen-cpp/EventLog_types.h	\
			     CircuitBreaker.h		\
			     ConfigFile.h		\
			     Hashing.h			\
			     FileLogger.h		\
			     ThruFileTransport.h	\
			     ReplicationRecorder.h	\
			     Spread.h			\
			     bloom_filter.hpp		\
			     utils.h

libthrucommon_la_SOURCES = \
			   gen-cpp/Redo.cpp			\
			   gen-cpp/Redo_types.cpp		\
			   gen-cpp/Redo_constants.cpp		\
			   gen-cpp/EventLog.cpp			\
			   gen-cpp/EventLog_types.cpp		\
			   gen-cpp/EventLog_constants.cpp	\
			   CircuitBreaker.cpp			\
			   ConfigFile.cpp			\
			   Hashing.cpp				\
			   FileLogger.cpp			\
			   ReplicationRecorder.cpp		\
			   Spread.cpp				\
			   ThruFileTransport.cpp

libthrucommon_la_CPPFLAGS = -Wall -Igen-cpp $(MEMCACHED_CFLAGS) $(SPREAD_CFLAGS) $(SSL_CFLAGS) $(THRIFT_CFLAGS) $(UUID_CFLAGS) $(BOOST_CPPFLAGS)
libthrucommon_la_LDFLAGS = -Wall $(SPREAD_LIBS) $(SSL_LIBS) $(THRIFT_LIBS) $(UUID_LIBS) $(MEMCACHED_LIBS) $(BOOST_LDFLAGS) $(LIBEVENT_LDFLAGS) $(LIBEVENT_LIBS) $(BOOST_FILESYSTEM_LIB)  $(BOOST_SYSTEM_LIB)

LDADD = $(top_builddir)/src/libthrucommon.la

thrurecorder_CPPFLAGS = -Wall -Igen-cpp $(SPREAD_CFLAGS) $(THRIFT_CFLAGS) $(BOOST_CPPFLAGS)
thrurecorder_LDADD = $(LDADD)
thrurecorder_LDFLAGS = -Wall -rpath $(pkglibdir) $(SPREAD_LIBS) $(THRIFTNB_LIBS) $(BOOST_LDFLAGS) 
thrurecorder_SOURCES = \
		       thrurecorder.cpp

gen-cpp/Redo.cpp: Redo.thrift
	${THRIFT} -cpp $<

gen-cpp/EventLog.cpp: EventLog.thrift
	${THRIFT} -cpp $<

clean-local:
	rm -fr gen-cpp
