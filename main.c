/* 
 * File:   main.c
 * Author: pixum
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
#define LOCAL_SERVER_PORT 9930
#define BUF 255
#define VERSION "YAUL version 0.1.0 - Yet another UDP logger"

int port = LOCAL_SERVER_PORT;
char *address = "0.0.0.0";
int daemonize = 0;
int comm[2];
char *mp = "0";
pid_t pid;

static void sig_hup(int signo) {
    syslog(LOG_INFO, "caught SIGHUP");
    //open_file();
}

static void sig_int(int signo) {
    syslog(LOG_INFO, "caught SIGINT");
    //close_file();
    syslog(LOG_INFO, "exiting");
    closelog();
    exit(EXIT_SUCCESS);
}

static void sig_term(int signo) {
    syslog(LOG_INFO, "caught SIGTERM");
    //close_file();
    syslog(LOG_INFO, "exiting");
    closelog();
    exit(EXIT_SUCCESS);
}

void print_version(void) {
    fprintf(stderr, "%s\n", VERSION);
}

void print_usage(void) {
    fprintf(stderr, "Usage: yaul [options]\n-h this help\n-d daemonize\n-p [port] Bind to port number\n-b [ip] Bind top ip address\n-v Version\n\n");
}

void daemonize_server(void) {
    char retcode = 0;
    
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
        mp = (char *)malloc(2);
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

/*
 * 
 */
int main(int argc, char** argv) {
    int s, rc, n, len;
	struct sockaddr_in cliAddr, servAddr;
	char puffer[BUF];
	time_t time1;
	char loctime[BUF];
	char *ptr;
	const int y = 1;
	int opt; 
    
    while ((opt = getopt(argc, (char ** const)argv, "b:dhp:v")) != EOF) {
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
            case 'd':
                printf("Daemonize process\n");
                daemonize = 1;
                break;
        }
    }
  
  // create socket
  s = socket (AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
     printf ("%s: cannot open socket(%s)\n", argv[0], strerror(errno));
     exit (EXIT_FAILURE);
  }
  // bind local address and port
  if(! inet_pton(AF_INET, address, &servAddr.sin_addr)) {
     (void)fprintf(stderr, "Invalid address: %s\n", address);
     exit(EXIT_FAILURE);
  }
  servAddr.sin_family = AF_INET;
  servAddr.sin_port = htons (port);
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));
  rc = bind ( s, (struct sockaddr *) &servAddr, sizeof (servAddr));
  if (rc < 0) {
     printf ("%s: cannot bind port %d (%s)\n", argv[0], port, strerror(errno));
     exit (EXIT_FAILURE);
  }
  
  printf ("Server listening on %s:%u (UDP)\n", address, port);
  
  if (daemonize == 1) {
      daemonize_server();
  }
  
  // main server loop (endless)
  while (1) {
    // init buffer/
    memset (puffer, 0, BUF);
    
    // receive messages
    len = sizeof (cliAddr);
    n = recvfrom ( s, puffer, BUF, 0,
                   (struct sockaddr *) &cliAddr, &len );
    if (n < 0) {
       printf ("%s: cannot receive data\n", argv[0] );
       continue;
    }
    
    // prepare timestamp
    time(&time1);
    strncpy(loctime, ctime(&time1), BUF);
    ptr = strchr(loctime, '\n' );
    *ptr = '\0';
    
    // output message
    printf ("%s: [%s:%u] %s \n",
            loctime, inet_ntoa (cliAddr.sin_addr),
            ntohs (cliAddr.sin_port), puffer);
  }
  return EXIT_SUCCESS;

}