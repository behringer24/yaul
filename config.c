/* 
 * Configuration handling
 * 
 * File:   config.c
 * Author: Andreas Behringer
 * 
 * (c)2012 Andreas Behringer
 * Copyright: GPL see included LICENSE file
 *
 * Created on January 10, 2013, 12:33 AM
 */

#ifdef	__cplusplus
extern "C" {
#endif
	
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
	
#include "config.h"

static struct option long_options[] = {
	{"config", required_argument, 0, 'c'},
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
	{"max-handles", required_argument, 0, 'm'},
	{0, 0, 0, 0}
};

/**
 * Print version string to screen
 */
void print_version(void) {
    fprintf(stdout, "YAUL version %s - Yet another UDP logger\n", VERSION);
}

/**
 * Print usage information to screen
 */
void print_usage(void) {
	print_version();
    fprintf(stdout, "Usage: yaul [options]\n\
-h, -?, --help             display this help information\n\
-c, --config               Load configuration from file (beta)\n\
-d, --daemonize            daemonize server process\n\
-p, --port=PORT            bind to port number (default %u)\n\
-b, --bind=IP              bind to ip address (default %s)\n\
-l, --logpath=PATH         logging to path (default %s)\n\
-s, --statistics=FREQUENCY log statistics to file yaul.stat after every [frequency] logmessage\n\
-f, --flush=FREQUENCY      flush output stream after every [frequency] logmessage\n\
-r, --redis-ip=IP          connect to redis server at IP and implicit enable logging to redis (default %s)\n\
-o, --redis-port=PORT      connect to redis server at PORT and implicit enable logging to redis (default %u)\n\
-t, --redis-ttl=TTL        the TTL in seconds of the dayly lists in redis, starting on last log message added, 0 = persist\n\
-m, --max-handles=NUM      maximum number of opened files (default %u)\n\
-v, --version              display version information\n", PORT, ADDRESS, LOGPATH, config.redis_ip, config.redis_port, MAXHANDLES);
}

/**
 * Init configuration with default values
 */
void setDefaultOptions(void) {
	config.configfile = NULL;
	config.address = ADDRESS;
	config.logpath = LOGPATH;
	config.maxhandles = MAXHANDLES;
	config.opt_daemonize = 0;
	config.opt_flush = FLUSH;
	config.opt_redis = 0;
	config.opt_statistics = 0;
	config.port = PORT;
	config.redis_ip = "127.0.0.1";
	config.redis_port = 6379;
	config.redis_ttl = 0;
}

int getOptChar(char * parameter) {
	int i = 0;
	
	while (long_options[i].name != 0 && strcmp(long_options[i].name, parameter) != 0) {
		i++;
	}
	
	if (long_options[i].name == 0) {
		return 0;
	} else {
		return long_options[i].val;
	}
}

void readConfigFile(char* filename) {
	FILE* fp = NULL;
	char row[1500] = "";
	char delimiter[] = "= \n\r";
	char *start = NULL;
	char *command, *param1, *param2;
	
	fp = fopen(filename, "r");
	if (fp == 0) {
		fprintf(stderr, "Cannot open config file \"%s\"", filename);
		exit(EXIT_FAILURE);
	}
	
	while (fgets(row, 1500, fp)) {
		start = row;
		
		while (isspace(*start)) {
			start++;
		}
		
		if (*start == 0 || *start == '#' || *start == '/' || *start == ';') {
			continue;
		}
		
		command = strtok(start, delimiter);
		param1 = strtok(NULL, delimiter);
		
		fprintf(stderr, "%s Command:%s Param:%s Char:%c\n", start, command, param1, getOptChar(command));
	}
	
	fclose(fp);
}

void handleConfigParameter(int opt, char * optarg) {
	switch (opt) {
		case 'c':
			config.configfile = optarg;
			break;
		case 'b':
			config.address = optarg;
			break;
		case 'p':
			config.port = atoi(optarg);
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
			config.logpath = optarg;
			break;
		case 's':
			config.opt_statistics = atoi(optarg);
			break;
		case 'f':
			config.opt_flush = atoi(optarg);
			break;
		case 'd':
			config.opt_daemonize = 1;
			break;
		case 'r':
			config.redis_ip = optarg;
			config.opt_redis = 1;
			break;
		case 'o':
			config.redis_port = atoi(optarg);
			config.opt_redis = 1;
			break;
		case 't':
			config.redis_ttl = atoi(optarg);
			config.opt_redis = 1;
			break;
		case 'm':
			config.maxhandles = atoi(optarg);
			break;
	}
}

/**
 * Read options from command line
 * @param argc
 * @param argv
 */
void readOptions(int argc, char** argv) {
	int opt;
	int opt_index;
	
	setDefaultOptions();
	
	// Parse options
    while ((opt = getopt_long(
			argc, 
			(char ** const)argv, 
			"c:b:dh?p:l:vs:f:r:o:t:m:", 
			long_options, 
			&opt_index)) != EOF) {
		handleConfigParameter(opt, optarg);
    }
	
	if (config.configfile) {
		readConfigFile(config.configfile);
	}
}
	
#ifdef	__cplusplus
}
#endif
