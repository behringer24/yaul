/* 
 * YAUL - yet another udp logger - main file
 * 
 * File:   yaul.c
 * Author: Andreas Behringer
 * 
 * (c)2012 Andreas Behringer
 * Copyright: GPL see included LICENSE file
 *
 * Created on November 19, 2012, 1:06 PM
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>

#include "hiredis/hiredis.h"
#include "hashtable/hashtable.h"
#include "hashtable/hashtable_itr.h"

#include "config.h"
#include "yaul.h"
#include "hash.h"

// general vars
struct handlebuffer * lastfile = NULL;		// last used file in handlebuffer
struct hashtable *handles;					// hashtable for buffering open filetables
redisContext *redis_context = NULL;			// context of opened redis connection
int sock = 0;								// the UDP socket
struct yaulConfig config;					// Configuration variable holder declaration

// statistic vars hold information since server start
unsigned int stat_messages_handled = 0;		// messages handled and stored
unsigned int stat_files_opened = 0;			// files opened
unsigned int stat_files_closed = 0;			// files closed
unsigned int stat_files_switched = 0;		// number of logfile switches
time_t stat_start_time = 0;					// timestamp server was started

/**
 * Compare function for hashtable key comparison
 * @param void * a
 * @param void * b
 * @return bool
 */
static int cmpKeys(void *a, void *b) {
	return (0 == strcmp(a, b));
}

/**
 * Dump hashtable to stderr, for debugging purposes
 * @param h
 */
void hashtableDump(struct hashtable *h) {
	struct hashtable_itr *itr;
	struct handlebuffer * handle;
	char * key;
	
	if (hashtable_count(h) > 0) {
		itr = hashtable_iterator(h);
		fprintf(stderr, "\nBegin dump\n");

		do {
			handle = hashtable_iterator_value(itr);
			key = hashtable_iterator_key(itr);
			fprintf(stderr, "Key %s: Filename %s\n", key, handle->name);
		} while (hashtable_iterator_advance(itr));

		free(itr);
	}
}

/**
 * close a random filehandle entry in the handlebuffer
 */
void closeRandomFile(void) {
	struct hashtable_itr *itr = NULL;
	struct handlebuffer * handle = NULL;
	unsigned int i = 0;
	unsigned int r = 0;
	
	if (!config.opt_redis) {
		// only run if more than one opened file
		if (hashtable_count(handles) > 1) {
			// do not close actual used file
			do {
				itr = hashtable_iterator(handles);
				r = rand() % (hashtable_count(handles));
				for (i = 0; i < r; i++) {
					hashtable_iterator_advance(itr);
				}
				handle = hashtable_iterator_value(itr);
				free(itr);
			} while (strcmp(handle->name, lastfile->name) == 0);
			
			fclose(handle->filehandle);
			stat_files_closed++;
			hashtable_remove(handles, handle->name);
		}
	}
}

/**
 * Close all opened filehandles in cache
 */
void closeAllFiles(void) {
	struct hashtable_itr *itr;
	struct handlebuffer * handle;
	
	itr = hashtable_iterator(handles);
	if (!config.opt_redis) {
		if (hashtable_count(handles) > 0) {
			do {
				handle = hashtable_iterator_value(itr);
				fclose(handle->filehandle);
				stat_files_closed++;
			} while (hashtable_iterator_remove(itr));			
		}
	}
	free(itr);
}

/**
 * Cleanup and exit
 */
void shutdownServer(void) {
	syslog(LOG_INFO, "exiting");
	closeAllFiles();
	hashtable_destroy(handles, 1);
    closelog();
    exit(EXIT_SUCCESS);
}

/**
 * Signal handler for SIGHUP
 * @param int signo
 */
static void sig_hup(int signo) {
    syslog(LOG_INFO, "caught SIGHUP");
	closeAllFiles();
}

/**
 * Signal handler for SIGINT
 * @param int signo
 */
static void sig_int(int signo) {
    syslog(LOG_INFO, "caught SIGINT");
    shutdownServer();
}

/**
 * Signal handler for SIGTERM
 * @param int signo
 */
static void sig_term(int signo) {
    syslog(LOG_INFO, "caught SIGTERM");
    shutdownServer();
}


/**
 * Daemonize the server process and terminate parent, call is configured by option -d
 */
void daemonize_server(void) {
    char retcode = 0;
	int comm[2];
	char mp[2];
	pid_t pid;
    
    // prepare pipe
    if(pipe(comm) == -1) {
        (void)perror("Unable to create pipe");
        exit(EXIT_FAILURE);
    }

    // do fork
    if((pid = fork()) < 0) {
        (void)perror("Unable to fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) { // child
        close(comm[0]);
        // bye to parent
        memset(mp, (char) EXIT_SUCCESS, 1);
        (void)write(comm[1], mp, 1);
        (void)close(comm[1]);
    } else { //parent
        close(comm[1]);
        // get bye from child
        if(read(comm[0], &retcode, 1) != 1) {
            (void)perror("Unable to read return code from child");
            exit(EXIT_FAILURE);
        }
        close(comm[0]);
		printf("Process daemonized\n");

        exit((int)retcode);
    }
    
    // Setup signal handlers
    if(signal(SIGHUP, sig_hup) == SIG_ERR) {
        exit(EXIT_FAILURE);
    }
    if(signal(SIGINT, sig_int) == SIG_ERR) {
        exit(EXIT_FAILURE);
    }
    if(signal(SIGTERM, sig_term) == SIG_ERR) {
        exit(EXIT_FAILURE);
    }
    
	openlog("yaul", 0, LOG_DAEMON|LOG_PID);
    syslog(LOG_INFO, "address %s, port %d", config.address, config.port);
}

/**
 * Open connection to Redis server
 * 
 * Errors are handled outside to keep server running if connection fails
 */
void openRedis(void) {
	struct timeval redis_timeout = { 1, 500000 }; // 1.5 seconds
	
	redis_context = redisConnectWithTimeout(config.redis_ip, config.redis_port, redis_timeout);
	if (redis_context->err) {
		syslog(LOG_ERR, "Redis connection error: %s\n", redis_context->errstr);
    }	
}

/**
 * Init server, open socket and syslog, bind socket to address and port
 */
void initServer(void) {
	struct sockaddr_in servAddr;
	const int y = 1;
	int rc;
	
	handles = create_hashtable(config.buffersize, djb2Hash, cmpKeys);
	
	sock = socket (AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("Cannot open socket\n");
		exit (EXIT_FAILURE);
	}
	// bind local address and port
	if(! inet_pton(AF_INET, config.address, &servAddr.sin_addr)) {
		(void)fprintf(stderr, "Invalid address: %s\n", config.address);
		exit(EXIT_FAILURE);
	}
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons (config.port);
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));
	rc = bind (sock, (struct sockaddr *) &servAddr, sizeof (servAddr));
	if (rc < 0) {
		fprintf (stderr, "cannot bind port %d\n", config.port);
		exit (EXIT_FAILURE);
	}

	printf ("YAUL listening on %s:%u (UDP)\n", config.address, config.port);
	
	if (config.opt_statistics > 0) {
		printf ("Statistics enabled to yaul.stat every %u message\n", config.opt_statistics);
	}
	
	if (config.opt_redis) {
		openRedis();
		if (redis_context->err) {
			fprintf(stderr, "Redis connection error: %s\n", redis_context->errstr);
			exit (EXIT_FAILURE);
		} else {
			printf("Logging to Redis enabled\n");
		}
	}

	if (config.opt_daemonize == 1) {
		daemonize_server();
	} else {
		openlog("yaul", 0, LOG_PID);
	}
	syslog(LOG_INFO, "Server started");
}


/**
 * Open logfile or return filehandle if file allready opened and in handles
 * @param char * name
 * @return FILE *
 */
FILE * openLogfile(char *name) {
	char filename[PATHLENGTH];
	struct handlebuffer * newfile = NULL;
	
	// If last message has same logname just return filehandle
	if (lastfile == NULL || strcmp(lastfile->name, name) != 0) {
		// implicit flush stream buffer of last logfile used if flushing is set > 1
		if (lastfile != NULL && config.opt_flush > 1 && lastfile->filehandle != NULL) {
			fflush(lastfile->filehandle);
		}
		
		// if not search linear in handles array
		newfile = hashtable_search(handles, name);		
		
		// if not found logname in handles array open file
		if (!newfile) {
			// check if max handles reached
			if (hashtable_count(handles) >= config.maxhandles) {
				closeRandomFile();
			}
			// open file and store in handles
			newfile = malloc(sizeof (struct handlebuffer));
			sprintf(filename, "%s/%s.log", config.logpath, name);
			newfile->filehandle = fopen(filename, "a");
			if (newfile->filehandle) {
				strcpy(newfile->name, name);
				hashtable_insert(handles, newfile->name, newfile);
				stat_files_opened++;
				stat_files_switched++;
			} else {
				free(newfile);
				free(lastfile);
			}
		} else {
			stat_files_switched++;
		}
		
		lastfile = newfile;
	}

	return lastfile->filehandle;
}

/**
 * Log message to file
 * 
 * @param char * name Name of logfile
 * @param char * message Message to be logged
 * @param char * loctime Timestring to prepend
 * @param char * address IP address of client
 * @param unsigned int port Port of client
 */
inline void logMessageFile(char *name, char *message) {
	FILE * fp = NULL;
	
	fp = openLogfile(name);
	if(fp != NULL) {
		fprintf(fp, "%s\n", message);
		stat_messages_handled++;
		// flush buffer immediately to allow tail -f on logfiles
		if (stat_messages_handled % config.opt_flush == 0) {
			fflush(fp);
		}
	} else {
		perror("Cannot open logfile");
		syslog(LOG_ERR, "Cannot open logfile: %s\n", name);
		exit(EXIT_FAILURE);
	}
}

/**
 * Log message to Redis
 * 
 * @param char * name Name of log
 * @param char * message Message to be logged
 * @param char * loctime Timestring to prepend
 * @param char * address IP address of client
 * @param unsigned int port Port of client
 */
inline void logMessageRedis(char *name, char *message) {
	redisReply *reply;
	char logtime[BUF];
	time_t rawtime;
	struct tm * timeinfo;
	
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(logtime, BUF, "%Y-%m-%d", timeinfo);
	
	// Write logline to redis list
    reply = redisCommand(redis_context, "LPUSH %s.%s %s", name, logtime, message);
	stat_messages_handled++;
	if (reply != NULL) {
		freeReplyObject(reply);
		if (config.redis_ttl > 0) {
			reply = redisCommand(redis_context, "EXPIRE %s.%s %u", name, logtime, config.redis_ttl);
			freeReplyObject(reply);
		}
		// @todo catch redis reply for ttl error
	} else {
		syslog(LOG_ERR, "Logging to redis failed at server %s:%u", config.redis_ip, config.redis_port);
		openRedis();
	}
}

/**
 * Log message from UDP socket depending on destination
 * 
 * @param char * buffer
 * @param char * address
 * @param unsigned int port
 */
inline void logMessage(char *buffer, char *address, unsigned int port) {
	char loctime[BUF];
	char message[MAXLENGTH];
	time_t rawtime;
	struct tm * timeinfo;
	char name[NAMELENGTH];
	
	// prepare timestamp
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(loctime, BUF, "%Y-%m-%d %H:%M:%S", timeinfo);
	
	// Scan for name of logfile / logname
	if (sscanf(buffer, "[%[a-zA-Z0-9.]]%[^\n]", name, buffer) != 2) {
		strcpy(name, "yaul");
	}
	
	// build standard logline
	sprintf(message, "%s [%s:%u] %s", loctime, address, port, buffer);
	
	if (config.opt_redis) {
		// output message to Redis
		logMessageRedis(name, message);
	} else {
		// output message to file
		logMessageFile(name, message);
	}
}

/**
 * Log statistic messages
 */
void statistics(void) {
	char statistic_message[BUF];
	
	sprintf(statistic_message, "[yaul.stat]messages:%u opened:%u closed:%u switched:%u running:%lu sec average/s:%f2\n",
			stat_messages_handled,
			stat_files_opened,
			stat_files_closed,
			stat_files_switched,
			(unsigned int) time(NULL) - stat_start_time,
			(double) stat_messages_handled / (time(NULL) - stat_start_time));
	logMessage(statistic_message, config.address, config.port);
}

/**
 * The UDP receiver loop, running forever, stopped by signals
 */
void serverLoop(void) {
	int len, n;
	char buffer[BUF];
	struct sockaddr_in cliAddr;
	
	while (1) {
		// init buffer
		memset (buffer, 0, BUF);

		// receive messages
		len = sizeof(cliAddr);
		n = recvfrom(sock, buffer, BUF, 0, (struct sockaddr *) &cliAddr, (socklen_t *) &len );
		if (n < 0) {
		   syslog(LOG_ERR, "cannot receive data");
		   continue;
		}

		// output message
		logMessage(buffer, inet_ntoa(cliAddr.sin_addr), ntohs(cliAddr.sin_port));
		
		// If optional statistics logging is set log every n'th message
		if (config.opt_statistics > 0 && stat_messages_handled % config.opt_statistics == 0) {
			statistics();
		}
	}
}

/**
 * Server main method
 * 
 * @param int argc
 * @param char ** argv
 * @return int
 */
int main(int argc, char** argv) {
	readOptions(argc, argv);
	
	// start statistics timer
	stat_start_time = time(NULL);
  
	// create socket and init server
	initServer();
  
	// start server loop (endless)
	serverLoop();
  
	return EXIT_SUCCESS;
}
