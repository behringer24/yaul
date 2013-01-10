/* 
 * Header definitions of main file
 * 
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

// Struct for buffering opened filehandles
typedef struct handlebuffer {
	char name[NAMELENGTH];
	FILE * filehandle;
} handlebuffer;

/* Function Prototypes */
static int cmpKeys(void *a, void *b);
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

