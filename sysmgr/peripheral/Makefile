# Copyright 2011 Google Inc. All Rights Reserved.
# Author: kedong@google.com (Ke Dong)

CXX=$(CROSS_COMPILE)g++
AR=$(CROSS_COMPILE)ar
RM=rm -f
CFLAGS=-fPIC -Wall -W -Wswitch-enum -DLOGGING=1
CXXFLAGS=-I.. -I../../base
OBJS=$(patsubst %.cc,%.o,$(wildcard *.cc))
PKG_CONFIG?=pkg-config

all: libbrunoperipheral.so


%.o: %.cc
	$(CXX) -c $(CFLAGS) $(CXXFLAGS) $< -o $@

libbrunoperipheral.so: $(OBJS)
	$(CXX) -shared \
		-Wl,-soname,libbrunoperipheral.so -Wl,-export-dynamic \
		-o $@ $^

clean:
	$(RM) *.o *.so *.a *~
