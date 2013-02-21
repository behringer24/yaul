/* 
 * Configuration handling header
 * 
 * File:   config.h
 * Author: Andreas Behringer
 * 
 * (c)2012 Andreas Behringer
 * Copyright: GPL see included LICENSE file
 * 
 * Created on January 10, 2013, 12:23 PM
 */

#ifndef CONFIG_H
#define	CONFIG_H

#ifdef	__cplusplus
extern "C" {
#endif

#define BUF 1500
#define MAXLENGTH 2000
#define NAMELENGTH 255
#define PATHLENGTH 2048
#define MAXHANDLES 50
#define FLUSH 1

// The following defines are usually set in Makefile
#ifndef PORT
#define PORT 9930
#endif

#ifndef ADDRESS
#define ADDRESS "0.0.0.0"
#endif

#ifndef LOGPATH
#define LOGPATH "/var/log/yaul"
#endif

#ifndef VERSION
#define VERSION "n/a"
#endif

#ifndef DEFAULTCFG
#define DEFAULTCFG "/etc/yaul.conf"
#endif
	
/**
 * Configuration variable holder
 */
struct yaulConfig {
	unsigned int port;					// the port yaul will be listening on
	char *address;						// the address the server is bound to
	char *configfile;					// path to configuration file
	unsigned int opt_daemonize;			// option: daemonize server
	unsigned int opt_statistics;		// option: write statistics
	unsigned int opt_flush;				// flush logfile buffer every n'th msg
	unsigned int opt_redis;				// log to redis instead of files
	char *redis_ip;						// ip address of redis server
	int redis_port;						// port of redis server
	int redis_ttl ;						// ttl of redis message lists
	char *logpath;						// the path to the logfiles
	unsigned int maxhandles;			// maximum number of opened files
};

extern struct yaulConfig config;

/* function declarations */
void print_version(void);
void print_usage(void);
void setDefaultOptions(void);
void readOptions(int argc, char** argv);
void handleConfigParameter(int opt, char * optarg);
int getOptChar(char * parameter);

#ifdef	__cplusplus
}
#endif

#endif	/* CONFIG_H */
