/*
 * Base64.h
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */

#ifndef BASE64_H_
#define BASE64_H_

/**
 * base64解码函数
 * @param in 输入的base64字符串
 * @param resultSize 解码后大小
 * @param trimTrailingZeros
 * @return 解码后的结果
 */
unsigned char* base64Decode(char const* in, unsigned& resultSize,
			    bool trimTrailingZeros = true);
    // returns a newly allocated array - of size "resultSize" - that
    // the caller is responsible for delete[]ing.

/**
 * base64编码
 * @param orig 原始数据
 * @param origLength 原始数据长度
 * @return 编码后的base64字符串
 */
char* base64Encode(char const* orig, unsigned origLength);
    // returns a 0-terminated string that
    // the caller is responsible for delete[]ing.


#endif /* BASE64_H_ */
