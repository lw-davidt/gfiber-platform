# Copyright 2011 Google Inc. All Rights Reserved.
# Author: kedong@google.com (Ke Dong)

CXX=$(CROSS_COMPILE)g++
RM=rm -f
INSTALL=install
CFLAGS=-Wall -Wimplicit -Wno-unknown-pragmas -W
CPPFLAGS=-I.. -I../peripheral -I../../base \
	$(subst -Wstrict-prototypes,,$(subst -std=c99,,$(shell $(PKG_CONFIG) --cflags brunobase)))
LDFLAGS=
PREFIX=/home/test
BINDIR=$(DESTDIR)$(PREFIX)
PKG_CONFIG?=pkg-config

OBJS=$(patsubst %.cc,%.o,$(wildcard *.cc))
EXES=$(patsubst %.o,%,$(OBJS))

all: $(EXES)

$(EXES): \
	../peripheral/libbrunoperipheral.so \
	../../base/libbrunobase.so

%: %.cc
	$(CXX) $(CFLAGS) $(LDFLAGS) $(CPPFLAGS) -o $@ $^ -lpthread

install:
	mkdir -p $(BINDIR)
	$(INSTALL) -m 0755 $(EXES) $(BINDIR)/

clean:
	$(RM) $(EXES)
