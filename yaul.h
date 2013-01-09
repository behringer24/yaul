/* 
 * File:   yaul.h
 * Author: Andreas Behringer
 * 
 * (c)2012 Andreas Behringer
 * Copyright: GPL see included LICENSE file
 *
 * Created on January 9, 2013, 1:28 PM
 */

#ifndef YAUL_H
#define	YAUL_H

#ifdef	__cplusplus
extern "C" {
#endif

#define BUF 1500
#define MAXLENGTH 2000
#define NAMELENGTH 255
#define PATHLENGTH 2048
#define HANDLEBUFFER 20
#define MAXHANDLES 20
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

// Struct for buffering opened filehandles
typedef struct handlebuffer {
	char name[NAMELENGTH];
	FILE * filehandle;
} handlebuffer;

/* Function Prototypes */
static int cmpKeys(void *a, void *b);
static unsigned int hashKey(void *k);
void closeRandomFile(void);
void closeAllFiles(void);
void shutdownServer(void);
static void sig_hup(int signo);
static void sig_int(int signo);
static void sig_term(int signo);
void print_version(void);
void print_usage(void);
void daemonize_server(void);
void openRedis(void);
void initServer(void);
FILE * openLogfile(char *name);
void logMessageFile(char *name, char *message);
void logMessageRedis(char *name, char *message);
void logMessage(char *buffer, char *address, unsigned int port);
void statistics(void);
void serverLoop(void);

/* Debug helpers */
void hashtableDump(struct hashtable *h);

#ifdef	__cplusplus
}
#endif

#endif	/* YAUL_H */

