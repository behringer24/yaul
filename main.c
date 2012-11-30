/* 
 * File:   main.c
 * Author: Andreas Behringer
 * 
 * (c)2012 Andreas Behringer
 * Copyright: GPL
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

#define BUF 1500
#define NAMELENGTH 255
#define PATHLENGTH 2048
#define HANDLEBUFFER 10

// For buffering opened filehandles
struct handlebuffer {
	char name[NAMELENGTH];
	FILE * filehandle;
};

// general vars
int port = PORT;
char *address = ADDRESS;
unsigned int opt_daemonize = 0;
unsigned int opt_statistics = 0;
int sock = 0;
char logpath[PATHLENGTH] = LOGPATH;
int lastfile = 0;
struct handlebuffer handles[10];

// statistic vars
unsigned int stat_messages_handled = 0;
unsigned int stat_files_opened = 0;
unsigned int stat_files_closed = 0;
unsigned int stat_files_switched = 0;
time_t stat_start_time = 0;

/**
 * Close all opened filehandles in cache
 */
void closeAllFiles(void) {
	int i;
	
	for(i = 0; i < HANDLEBUFFER; i++) {
		if (handles[i].filehandle) {
			fclose(handles[i].filehandle);
			stat_files_closed++;
		}
	}
}

/**
 * Signal handler for SIGHUP
 * @param signo
 */
static void sig_hup(int signo) {
    syslog(LOG_INFO, "caught SIGHUP");
	closeAllFiles();
	memset(handles, 0, sizeof(handles));
}

/**
 * Signal handler for SIGINT
 * @param signo
 */
static void sig_int(int signo) {
    syslog(LOG_INFO, "caught SIGINT");
    syslog(LOG_INFO, "exiting");
	closeAllFiles();
    closelog();
    exit(EXIT_SUCCESS);
}

/**
 * Signal handler for SIGTERM
 * @param signo
 */
static void sig_term(int signo) {
    syslog(LOG_INFO, "caught SIGTERM");
    syslog(LOG_INFO, "exiting");
	closeAllFiles();
    closelog();
    exit(EXIT_SUCCESS);
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
    fprintf(stderr, "Usage: yaul [options]\n\
-h this help\n\
-d daemonize\n\
-p [port] Bind to port number (default %u)\n\
-b [ip] Bind top ip address (default %s)\n\
-l [path] Logging to path (default %s)\n\
-s [frequency] log statistics to file yaul.stat every [frequency] logmessage\n\
-v Version\n", PORT, ADDRESS, LOGPATH);
}

/**
 * daemonize the server process and terminate parent, call is configured by option -d
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
 * Init server, open socket and syslog, bind socket to address and port
 */
void initServer(void) {
	struct sockaddr_in servAddr;
	const int y = 1;
	int rc;
	
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
	int i = 0;
	
	// If last message hat same logname just return filehandle
	if (strcmp(handles[lastfile].name, name) != 0) {
		// if not search linear in handles array
		while (i < HANDLEBUFFER && strcmp(handles[i].name, name) != 0) {
			i++;
		}
		
		// if not found logname in handles array open file
		if (i == HANDLEBUFFER) {
			lastfile++;
			if (lastfile > HANDLEBUFFER - 1) {
				lastfile = 0;
			}

			// if position contains opened filehandle close it
			if (handles[lastfile].filehandle) {
				fclose(handles[lastfile].filehandle);
				stat_files_closed++;
			}

			// open file and store in handles
			sprintf(filename, "%s/%s.log", logpath, name);
			strcpy(handles[lastfile].name, name);
			handles[lastfile].filehandle = fopen(filename, "a");
			stat_files_opened++;
			stat_files_switched++;
		} else {
			lastfile = i;
			stat_files_switched++;
		}
	}
	
	return handles[lastfile].filehandle;
}

/**
 * Log message to udp socket
 * @param char * buffer
 * @param char * address
 * @param unsigned int port
 */
void logMessage(char *buffer, char *address, unsigned int port) {
	FILE * fp = NULL;
	char loctime[BUF];
	time_t time1;
	char *ptr;
	char name[NAMELENGTH];
	
	// prepare timestamp
	time(&time1);
	strncpy(loctime, ctime(&time1), BUF);
	ptr = strchr(loctime, '\n' );
	*ptr = '\0';
	
	if (sscanf(buffer, "[%[a-zA-Z0-9.]]%[^\n]", name, buffer) != 2) {
		strcpy(name, "yaul");
	}
	
	// output message to file
	fp = openLogfile(name);
	if(fp != NULL) {
		fprintf(fp, "%s: [%s:%u] %s\n",
				loctime,
				address,
				port,
				buffer);
		stat_messages_handled++;
		// flush buffer immediately to allow tail -f on logfiles
		fflush(fp);
	} else {
		perror("Cannot open logfile");
		exit(EXIT_FAILURE);
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
 * @param argc
 * @param argv
 * @return int
 */
int main(int argc, char** argv) {
	int opt;
	
	memset(handles, 0, sizeof(handles));
	stat_start_time = time(NULL);
	
	// Parse options
    while ((opt = getopt(argc, (char ** const)argv, "b:dhp:l:vs:")) != EOF) {
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
			case 'd':
				opt_daemonize = 1;
				break;
		}
    }
  
	// create socket and init server
	initServer();
  
	// start server loop (endless)
	serverLoop();
  
	return EXIT_SUCCESS;
}
