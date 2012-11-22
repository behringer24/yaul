# yaul Makefile

# Environment 
PREFIX	= /usr/local
MKDIR	= mkdir
CP	= cp
CC	= gcc
CCADMIN	= CCadmin
FLAGS	= -Wall -MMD -MP
DFLAGS  = -g
RFLAGS  = -O2
CONF	= Release

all: yaul-$(CONF)
	
debug: yaul-Debug
	
yaul-Release: main.c
	echo Target: $(CONF)
	$(CC) $(FLAGS) $(RFLAGS) -o yaul main.c
	
yaul-Debug: main.c
	echo Target: $(CONF)
	$(CC) $(FLAGS) $(DFLAGS) -o yaul main.c

clean:
	rm yaul
	rm yaul.d