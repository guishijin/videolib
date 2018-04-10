/*
 * strDup.h
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */

#ifndef STRDUP_H_
#define STRDUP_H_


/**
 * 复制字符串
 * @param str 源字符串
 * @return 复制后的字符串
 */
char* strDup(char const* str);
// Note: strDup(NULL) returns NULL

/**
 * 分配一个和源字符串大小相同的缓冲区，但是不拷贝数据内容。
 * @param str 源字符串
 * @return 分配的缓冲区
 */
char* strDupSize(char const* str);
// Like "strDup()", except that it *doesn't* copy the original.
// (Instead, it just allocates a string of the same size as the original.)


#endif /* STRDUP_H_ */
