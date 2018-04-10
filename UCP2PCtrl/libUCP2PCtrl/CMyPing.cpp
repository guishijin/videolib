/*
 * CMyPing.cpp
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */

#include "CMyPing.h"

//#include <winsock2.h>
//#include <iphlpapi.h>
//#include <icmpapi.h>

#include <stdio.h>



////指定与静态库一起连接
//#pragma comment(lib,"Iphlpapi.lib")

//测试字符串
const char* chPingString = "Test Net - Sos Admin";

//构造函数
CMyPing::CMyPing()
{
//	//句柄
//	hICMPHandle = NULL;
//	//发送数据长度
//	nSendLen = (WORD)strlen(chPingString);
//	//分配空间
//	pInfo = new Information;
//	pEcho = new char[sizeof(TIcmpEchoReply) + 40];
}
//析构函数
CMyPing::~CMyPing()
{
//	//如果没有关闭句柄，则关闭句柄
//	if(hICMPHandle != NULL)
//	{
//		IcmpCloseHandle(hICMPHandle);
//		hICMPHandle = NULL;
//	}
//	//释放空间
//	if(pInfo != NULL)
//	{
//		delete pInfo;
//		pInfo = NULL;
//	}
//	if(pEcho != NULL)
//	{
//		delete pEcho;
//		pEcho = NULL;
//	}
}

//初始化函数，加载动态库
bool CMyPing::Init()
{
//	//如果句柄不为空，直接返回成功
//	if(hICMPHandle != NULL)
//		return true;
//	//创建句柄
//	hICMPHandle = IcmpCreateFile();
//	//如果句柄为空，返回失败
//	if(hICMPHandle == NULL)
//		return false;
	return true;
}

/**
 * 假的实现，永远返回1
 * @param chIP
 * @param nOverTime
 * @param nTimes
 * @return
 */
unsigned int CMyPing::SendPing(char* chIP, unsigned int nOverTime,unsigned int nTimes)
{
	return 1;
}

////ping测试命令发送
////chIP		目标IP地址
////nOverTime	超时时间
//DWORD CMyPing::SendPing(char* chIP, DWORD nOverTime,DWORD nTimes)
//{
//	//创建句柄
//	if(hICMPHandle == NULL)
//	{
//		hICMPHandle = IcmpCreateFile();
//	}
//	if(hICMPHandle == NULL)
//	{
//		return (0);
//	}
//	//返回值
//	nResult = 0;
//	//转化IP地址类型
//	nIPAddress = inet_addr(chIP);
//	//获取数据长度
//	nReplySize = sizeof(TIcmpEchoReply) + 40;
//	//分配缓冲区
//	pReplyBuf = new byte[nReplySize];
//	for(DWORD i=0;i<nTimes;i++)
//	{
//		//发送信息结构
//		memset(pInfo,0x00,sizeof(Information));
//		pInfo->TTL = 64;
//		//回应信息结构
//		memset(pEcho,0x00,sizeof(TIcmpEchoReply) + 40);
//		((TIcmpEchoReply*)pEcho)->Data = pReplyBuf;
//		//发送ping命令
//		PingReply = IcmpSendEcho(hICMPHandle, nIPAddress, (byte*)chPingString, nSendLen,(PIP_OPTION_INFORMATION)pInfo, pEcho, nReplySize, nOverTime);
//		//判断回应数据是否正确
//		if((((TIcmpEchoReply*)pEcho)->DataSize == 20) && (memcmp(chPingString,((TIcmpEchoReply*)pEcho)->Data,nSendLen) == 0))//2011-11-27 10:56:00 lyf
//		{
//			//返回结果赋值
//			nResult= 1;
//			//退出循环
//			break;
//		}
//	}
//	//释放内存
//	delete pReplyBuf;pReplyBuf = NULL;
//	//返回结果
//	return nResult;
//}

