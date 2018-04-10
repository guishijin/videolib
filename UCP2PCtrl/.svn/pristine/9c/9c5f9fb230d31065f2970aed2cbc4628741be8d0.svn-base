/*
 * CMyPing.h
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */

#ifndef CMYPING_H_
#define CMYPING_H_


////数据信息结构
// typedef struct{
//	 byte TTL;
//	 byte TOS;
//	 byte Flags;
//	 byte OptionsSize;
//	 byte* OptionsData;
// }Information;
//
////ICMP信息结构
// typedef struct{
//	 DWORD Address;
//	 DWORD Status;
//	 DWORD RTT;
//	 WORD DataSize;
//	 WORD Reserved;
//	 byte*  Data;
//	 Information Options;
// }TIcmpEchoReply;

class CMyPing{
public:
	CMyPing();
	~CMyPing();
private:
//	//ICMP句柄
//	HANDLE hICMPHandle;
//
//	//数据信息结构
//	Information*	pInfo;
//	char*	pEcho;
//	//接收IcmpSendEcho返回值的变量
//	DWORD			PingReply;
//	//目标IP地址
//	DWORD			nIPAddress;
//	//测试发送数据长度
//	WORD			nSendLen;
//	//接收回应数据缓冲区指针
//	byte*			pReplyBuf;
//	//接收回应数据长度
//	DWORD			nReplySize;
//	//ping测试命令的返回值
//	DWORD			nResult;
public:
	//初始化函数，加载动态库
	bool Init();
	//发送ping测试命令
	//DWORD SendPing(char* chIP,DWORD nOverTime,DWORD nTimes);
	unsigned int SendPing(char* chIP, unsigned int nOverTime,unsigned int nTimes);
};


#endif /* CMYPING_H_ */
