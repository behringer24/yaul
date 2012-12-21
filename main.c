/* 
 * File:   main.c
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
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>

#include "hiredis/hiredis.h"
#include "hashtable/hashtable.h"
#include "hashtable/hashtable_itr.h"

#define BUF 1500
#define NAMELENGTH 255
#define PATHLENGTH 2048
#define HANDLEBUFFER 10
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
struct handlebuffer {
	char name[NAMELENGTH];
	FILE * filehandle;
};

// general vars
int port = PORT;							// the port yaul will be listening on
char *address = ADDRESS;					// the address the server is bound to
unsigned int opt_daemonize = 0;				// option: daemonize server
unsigned int opt_statistics = 0;			// option: write statistics
unsigned int opt_flush = FLUSH;				// flush logfile buffer every n'th msg
unsigned int opt_redis = 0;					// log to redis instead of files
redisContext *redis_context = NULL;			// context of opened redis connection
char *redis_ip = "127.0.0.1";				// ip address of redis server
int redis_port = 6379;						// port of redis server
int redis_ttl = 0;							// ttl of redis message lists
struct timeval redis_timeout = { 1, 500000 }; // 1.5 seconds
int sock = 0;								// the UDP socket
char logpath[PATHLENGTH] = LOGPATH;			// the path to the logfiles
int buffersize = HANDLEBUFFER;
struct handlebuffer * lastfile = NULL;		// last used file in handlebuffer
//struct handlebuffer handles[HANDLEBUFFER];	// buffer of opened filehandles
struct hashtable *handles;

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
 * Hashfunction
 * @param void * k
 * @return int
 */
static unsigned int hashKey(void *k) {
	int i = 0;
	int h = 0;
	char *key = (char *) k;
	
	for (i=0; i<strlen(key); i++) {
		h += h * 256 + key[i];
	}
	
	return h;
}

/**
 * Close all opened filehandles in cache
 */
void closeAllFiles(void) {
	struct hashtable_itr *itr;
	struct handlebuffer * handle;
	
	itr = hashtable_iterator(handles);
	if (!opt_redis) {
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
 * Print version string to screen
 */
void print_version(void) {
    fprintf(stderr, "YAUL version %s - Yet another UDP logger\n", VERSION);
}

/**
 * Print usage information to screen
 */
void print_usage(void) {
	print_version();
    fprintf(stderr, "Usage: yaul [options]\n\
-h, -?, --help             display this help information\n\
-d, --daemonize            daemonize server process\n\
-p, --port=PORT            bind to port number (default %u)\n\
-b, --bind=IP              bind to ip address (default %s)\n\
-l, --logpath=PATH         logging to path (default %s)\n\
-s, --statistics=FREQUENCY log statistics to file yaul.stat after every [frequency] logmessage\n\
-f, --flush=FREQUENCY      flush output stream after every [frequency] logmessage\n\
-r, --redis-ip=IP          connect to redis server at IP and implicit enable logging to redis (default %s)\n\
-o, --redis-port=PORT      connect to redis server at PORT and implicit enable logging to redis (default %u)\n\
-t, --redis-ttl=TTL        the TTL in seconds of the dayly lists in redis, starting on last log message added, 0 = persist\n\
-v, --version              display version information\n", PORT, ADDRESS, LOGPATH, redis_ip, redis_port);
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
            (void)perror("Unable to read return code");
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
    syslog(LOG_INFO, "address %s, port %d", address, port);
}

/**
 * Open connection to Redis server
 * 
 * Errors are handled outside to keep server running if connection fails
 */
void openRedis(void) {
	redis_context = redisConnectWithTimeout(redis_ip, redis_port, redis_timeout);
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
	
	handles = create_hashtable(buffersize, hashKey, cmpKeys);
	
	sock = socket (AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("Cannot open socket\n");
		exit (EXIT_FAILURE);
	}
	// bind local address and port
	if(! inet_pton(AF_INET, address, &servAddr.sin_addr)) {
		(void)fprintf(stderr, "Invalid address: %s\n", address);
		exit(EXIT_FAILURE);
	}
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons (port);
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));
	rc = bind (sock, (struct sockaddr *) &servAddr, sizeof (servAddr));
	if (rc < 0) {
		fprintf (stderr, "cannot bind port %d\n", port);
		exit (EXIT_FAILURE);
	}

	printf ("YAUL listening on %s:%u (UDP)\n", address, port);
	
	if (opt_statistics > 0) {
		printf ("Statistics enabled to yaul.stat every %u message\n", opt_statistics);
	}
	
	if (opt_redis) {
		openRedis();
		if (redis_context->err) {
			fprintf(stderr, "Redis connection error: %s\n", redis_context->errstr);
			exit (EXIT_FAILURE);
		} else {
			printf("Logging to Redis enabled\n");
		}
	}

	if (opt_daemonize == 1) {
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
	
	// If last message hat same logname just return filehandle
	if (lastfile == NULL || strcmp(lastfile->name, name) != 0) {
		// implicit flush stream buffer of last logfile used if flushing is set > 1
		if (opt_flush > 1 && lastfile->filehandle != NULL) {
			fflush(lastfile->filehandle);
		}
		
		// if not search linear in handles array
		lastfile = hashtable_search(handles, name);		
		
		// if not found logname in handles array open file
		if (!lastfile) {
			// open file and store in handles
			lastfile = malloc(sizeof (struct handlebuffer));
			sprintf(filename, "%s/%s.log", logpath, name);
			lastfile->filehandle = fopen(filename, "a");
			if (lastfile->filehandle) {
				strcpy(lastfile->name, name);
				hashtable_insert(handles, name, lastfile);
				stat_files_opened++;
				stat_files_switched++;
			} else {
				free(lastfile);
			}
		} else {
			stat_files_switched++;
		}
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
void logMessageFile(char *name, char *message, char *loctime, char *address, unsigned int port) {
	FILE * fp = NULL;
	
	fp = openLogfile(name);
	if(fp != NULL) {
		fprintf(fp, "%s: [%s:%u] %s\n",
				loctime,
				address,
				port,
				message);
		stat_messages_handled++;
		// flush buffer immediately to allow tail -f on logfiles
		if (stat_messages_handled % opt_flush == 0) {
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
void logMessageRedis(char *name, char *message, char *loctime, char *address, unsigned int port) {
	redisReply *reply;
	char logtime[BUF];
	time_t rawtime;
	struct tm * timeinfo;
	
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(logtime, BUF, "%Y-%m-%d", timeinfo);
	
	//sprintf(message, "%s %s", loctime, message);
	
	/* PING server */
    reply = redisCommand(redis_context, "LPUSH %s.%s %s>%s", name, logtime, loctime, message);
	stat_messages_handled++;
	if (reply != NULL) {
		freeReplyObject(reply);
		if (redis_ttl > 0) {
			reply = redisCommand(redis_context, "EXPIRE %s.%s %u", name, logtime, redis_ttl);
			freeReplyObject(reply);
		}
		// @todo catch redis reply for ttl error
	} else {
		syslog(LOG_ERR, "Logging to redis failed at server %s:%u", redis_ip, redis_port);
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
void logMessage(char *buffer, char *address, unsigned int port) {
	char loctime[BUF];
	time_t rawtime;
	struct tm * timeinfo;
	char name[NAMELENGTH];
	
	// prepare timestamp
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(loctime, BUF, "%Y-%m-%d %H:%M:%S", timeinfo);
	
	if (sscanf(buffer, "[%[a-zA-Z0-9.]]%[^\n]", name, buffer) != 2) {
		strcpy(name, "yaul");
	}
	
	if (opt_redis) {
		// output message to Redis
		logMessageRedis(name, buffer, loctime, address, port);
	} else {
		// output message to file
		logMessageFile(name, buffer, loctime, address, port);
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
	logMessage(statistic_message, address, port);
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
		if (opt_statistics > 0 && stat_messages_handled % opt_statistics == 0) {
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
	int opt;
	int opt_index;
	
	static struct option long_options[] = {
		{"bind", required_argument, 0, 'b'},
		{"port", required_argument, 0, 'p'},
		{"version", no_argument, 0, 'v'},
		{"help", no_argument, 0, 'h'},
		{"logpath", required_argument, 0, 'l'},
		{"statistics", required_argument, 0, 's'},
		{"flush", required_argument, 0, 'f'},
		{"daemonize", no_argument, 0, 'd'},
		{"redis-ip", required_argument, 0, 'r'},
		{"redis-port", required_argument, 0, 'o'},
		{"redis-ttl", required_argument, 0, 't'},
		{0, 0, 0, 0}
	};
		
	stat_start_time = time(NULL);
	
	// Parse options
    while ((opt = getopt_long(
			argc, 
			(char ** const)argv, 
			"b:dh?p:l:vs:f:r:o:t:", 
			long_options, 
			&opt_index)) != EOF) {
		switch (opt) {
			case 'b':
				address = optarg;
				break;
			case 'p':
				port = atoi(optarg);
				break;
			case 'v':
				print_version();
				exit (EXIT_SUCCESS);
				break;
			case '?':
			case 'h':
				print_usage();
				exit (EXIT_SUCCESS);
				break;
			case 'l':
				strcpy(logpath, optarg);
				break;
			case 's':
				opt_statistics = atoi(optarg);
				break;
			case 'f':
				opt_flush = atoi(optarg);
				break;
			case 'd':
				opt_daemonize = 1;
				break;
			case 'r':
				redis_ip = optarg;
				opt_redis = 1;
				break;
			case 'o':
				redis_port = atoi(optarg);
				opt_redis = 1;
				break;
			case 't':
				redis_ttl = atoi(optarg);
				opt_redis = 1;
				break;
		}
    }
  
	// create socket and init server
	initServer();
  
	// start server loop (endless)
	serverLoop();
  
	return EXIT_SUCCESS;
}
