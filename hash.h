/* 
 * Hash function header
 * 
 * File:   hash.h
 * Author: Andreas Behringer
 * 
 * (c)2012 Andreas Behringer
 * Copyright: GPL see included LICENSE file
 *
 * Created on January 10, 2013, 4:12 PM
 */

#ifndef HASH_H
#define	HASH_H

#ifdef	__cplusplus
extern "C" {
#endif

unsigned int djb2Hash(void *k);
unsigned int sdbmHash(void *k);

#ifdef	__cplusplus
}
#endif

#endif	/* HASH_H */

