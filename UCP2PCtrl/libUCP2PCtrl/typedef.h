/*
 * typedef.h
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */

#ifndef TYPEDEF_H_
#define TYPEDEF_H_

#include <pthread.h>

typedef char             int8;
typedef short            int16;
typedef long             int32;
typedef unsigned char    uint8;
typedef unsigned char    byte;
typedef unsigned short   uint16;
typedef unsigned long    uint32;

typedef unsigned char  BYTE;
typedef unsigned int  DWORD;
typedef unsigned int UINT;

typedef void * LPVOID;

typedef pthread_t HANDLE;

typedef bool BOOL;
#define  TRUE true
#define  FALSE false

#endif /* TYPEDEF_H_ */
