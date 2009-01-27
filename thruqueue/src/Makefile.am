bin_PROGRAMS = thruqueue

thruqueue_SOURCES = \
		  gen-cpp/Thruqueue.cpp			\
		  gen-cpp/Thruqueue_types.cpp		\
		  gen-cpp/Thruqueue_constants.cpp	\
		  gen-cpp/QueueLog.cpp			\
		  Queue.cpp				\
		  QueueManager.cpp			\
		  ThruqueueHandler.cpp			\
		  main.cpp

LDADD = ../../thrucommon/src/libthrucommon.la

thruqueue_CPPFLAGS = -Wall -Igen-cpp $(THRIFTNB_CFLAGS) -I../../thrucommon/src -I../../thrucommon/src/gen-cpp $(SSL_CFLAGS) $(UUID_CFLAGS) $(BOOST_CPPFLAGS) $(LIBEVENT_CPPFLAGS)
thruqueue_LDFLAGS = -Wall $(THRIFTNB_LIBS) $(SSL_LIBS) $(UUID_LIBS) $(BOOST_LDFLAGS) $(LIBEVENT_LDFLAGS) $(BOOST_FILESYSTEM_LIB) -lboost_system-mt
thruqueue_LDADD = $(LDADD)
thruqueue_DEPENDENCIES = $(LDADD)

gen-cpp/Thruqueue.cpp: Thruqueue.thrift
	${THRIFT} -cpp $<

clean-local:
	rm -fr gen-cpp
