/*
 * our_md5.h
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */

#ifndef OUR_MD5_H_
#define OUR_MD5_H_


typedef unsigned UNSIGNED32;

/* Definitions of _ANSI_ARGS and EXTERN that will work in either
   C or C++ code:
 */
//#undef _ANSI_ARGS_
//#if ((defined(__STDC__) || defined(SABER)) && !defined(NO_PROTOTYPE)) || defined(__cplusplus) || defined(USE_PROTOTYPE)
//#   define _ANSI_ARGS_(x)       x
//#else
//#   define _ANSI_ARGS_(x)       ()
//#endif
//#ifdef __cplusplus
//#   define EXTERN extern "C"
//#else
//#   define EXTERN extern
//#endif

/* MD5 context. */
typedef struct MD5Context {
  UNSIGNED32 state[4];	/* state (ABCD) */
  UNSIGNED32 count[2];	/* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];	/* input buffer */
} MD5_CTX;

//EXTERN void   our_MD5Init (MD5_CTX *);
//EXTERN void   ourMD5Update (MD5_CTX *, const unsigned char *, unsigned int);
//EXTERN void   our_MD5Pad (MD5_CTX *);
//EXTERN void   our_MD5Final (unsigned char [16], MD5_CTX *);
//EXTERN char * our_MD5End(MD5_CTX *, char *);
//EXTERN char * our_MD5File(const char *, char *);
//EXTERN char * our_MD5Data(const unsigned char *, unsigned int, char *);

 void   our_MD5Init (MD5_CTX *);
 void   ourMD5Update (MD5_CTX *, const unsigned char *, unsigned int);
 void   our_MD5Pad (MD5_CTX *);
 void   our_MD5Final (unsigned char [16], MD5_CTX *);
 char * our_MD5End(MD5_CTX *, char *);
 char * our_MD5File(const char *, char *);
char * our_MD5Data(const unsigned char *, unsigned int, char *);


#endif /* OUR_MD5_H_ */
