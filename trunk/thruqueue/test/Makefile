# Makefile for test
# 
# Author:
#   T Jake Luciani <jake@3.rdrail.net>

# Default target is everything
target: all

# Tools
THRIFT=thrift

all: ../src/Thruqueue.thrift
	$(THRIFT) --gen perl ../src/Thruqueue.thrift

clean:
	rm -fr gen-perl
