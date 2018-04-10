/*
 * strDup.cpp
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */

#include "strDup.h"

#include <string.h>

/**
 * 字符串复制函数
 * @param str 源字符串
 * @return 复制的新字符串。注意，新字符串需要外部使用delete[]删除。
 */
char* strDup(char const* str)
{
	if (str == NULL) return NULL;
	size_t len = strlen(str) + 1;
	char* copy = new char[len];

	if (copy != NULL)
	{
		memcpy(copy, str, len);
	}
	return copy;
}

/**
 * 根据指定的字符串，分配相同大小的空间
 * @param str 参考字符串
 * @return 复制的空间
 */
char* strDupSize(char const* str)
{
	if (str == NULL) return NULL;
	size_t len = strlen(str) + 1;
	char* copy = new char[len];

	return copy;
}
