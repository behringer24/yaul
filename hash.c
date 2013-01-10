/* 
 * Hash function
 * 
 * File:   hash.c
 * Author: Andreas Behringer
 * 
 * (c)2012 Andreas Behringer
 * Copyright: GPL see included LICENSE file
 *
 * Created on January 10, 2013, 4:12 PM
 */

#ifdef	__cplusplus
extern "C" {
#endif
	
#include <string.h>

#include "hash.h"

/**
 * D.J. Bernsteins hash algorithm
 * 
 * @param void * k
 * @return unsigned int
 */
unsigned int djb2Hash(void *k) {
	unsigned long h = 0;
	int c;
	char *key = (char *) k;
	
	while ((c = *key++)) {
		h = ((h << 5) + h) + c;
	}
	
	return h;
}

/**
 * BDB Berkeley Database hash algorithm
 * 
 * @param void * k
 * @return unsigned int
 */
unsigned int sdbmHash(void *k) {
	unsigned long h = 0;
	int c;
	char *key = (char *) k;
	
	while ((c = *key++)) {
		h = c + (h << 6) + (h << 16) - h;
	}
	
	return h;
}

#ifdef	__cplusplus
}
#endif
