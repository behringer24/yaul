# YAUL Makefile
# Author: Andreas Behringer
# 
# (c)2012 Andreas Behringer
# Copyright: GPL see included LICENSE file

# Environment 
VERSION = "1.0.1"
PREFIX	= /usr
LOGPATH	= "/var/log/yaul"
ADDRESS	= "0.0.0.0"
PORT	= 9930
MKDIR	= mkdir
CHMOD	= chmod
CP	= cp
CC	= gcc
CCADMIN	= CCadmin
DEPS    = hiredis/hiredis.c hiredis/net.c hiredis/sds.c hashtable/hashtable.c hashtable/hashtable_itr.c config.c hash.c
FLAGS	= -Wall -MMD -MP -DVERSION='$(VERSION)' -DLOGPATH='$(LOGPATH)' -DPORT=$(PORT) -DADDRESS='$(ADDRESS)'
DFLAGS  = -g
RFLAGS  = -O2
CONF	= Release

all: yaul-$(CONF)
	
debug: yaul-Debug
	
install:
	install -m 755 yaul $(PREFIX)/sbin/yaul
	$(MKDIR) -p -m 777 $(LOGPATH)
	@echo Installation complete
	
yaul-Release: yaul.c
	@echo Target: $(CONF)
	$(CC) $(FLAGS) $(RFLAGS) -o yaul yaul.c $(DEPS) -lm
	@echo Build complete
	
yaul-Debug: yaul.c
	@echo Target: $(CONF)
	$(CC) $(FLAGS) $(DFLAGS) -o yaul yaul.c $(DEPS) -lm
	@echo Build complete

clean:
	rm yaul
	rm yaul.d

test:
	@echo Starting test	
	yaul --version
	@echo Test end