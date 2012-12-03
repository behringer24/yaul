# yaul Makefile

# Environment 
VERSION = "0.3.0"
PREFIX	= /usr
LOGPATH	= "/var/log/yaul"
ADDRESS	= "0.0.0.0"
PORT	= 9930
MKDIR	= mkdir
CHMOD	= chmod
CP	= cp
CC	= gcc
CCADMIN	= CCadmin
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
	
yaul-Release: main.c
	@echo Target: $(CONF)
	$(CC) $(FLAGS) $(RFLAGS) -o yaul main.c
	
yaul-Debug: main.c
	@echo Target: $(CONF)
	$(CC) $(FLAGS) $(DFLAGS) -o yaul main.c

clean:
	rm yaul
	rm yaul.d