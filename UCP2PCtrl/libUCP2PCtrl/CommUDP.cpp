/*
 * CommUDP.cpp
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */

#include "CommUDP.h"

#include <arpa/inet.h>

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdlib.h>
#include <string.h>

/**
 * 设置非阻塞接收
 */
static bool makeSocketNonBlocking(int sock)
{
#if defined(__WIN32__) || defined(_WIN32)
  unsigned long arg = 1;
  return ioctlsocket(sock, FIONBIO, &arg) == 0;
#elif defined(VXWORKS)
  int arg = 1;
  return ioctl(sock, FIONBIO, (int)&arg) == 0;
#else
  int curFlags = fcntl(sock, F_GETFL, 0);
  return fcntl(sock, F_SETFL, curFlags|O_NONBLOCK) >= 0;
#endif
}

/**
 * 设置阻塞接收
 */
static bool makeSocketBlocking(int sock)
{
  bool result;
#if defined(__WIN32__) || defined(_WIN32)
  unsigned long arg = 0;
  result = ioctlsocket(sock, FIONBIO, &arg) == 0;
#elif defined(VXWORKS)
  int arg = 0;
  result = ioctl(sock, FIONBIO, (int)&arg) == 0;
#else
  int curFlags = fcntl(sock, F_GETFL, 0);
  result = fcntl(sock, F_SETFL, curFlags&(~O_NONBLOCK)) >= 0;
#endif
  return result;
}

/*
函数原型：int UDP_Init()
参数	：无
返回值：1：初始化成功；0：初始化失败
函数功能：初始化，使用时要先调用初始化函数
*/
int UDP_Init()
{
#if defined(__WIN32__) || defined(_WIN32)
	WORD VersionRequested;
	WSADATA WsaData;
	//socket版本为2.2版
	VersionRequested=MAKEWORD(2,2);
	if(WSAStartup(VersionRequested,&WsaData) != 0)
	{
		return 0;
	} else
	{
		//版本不对，返回失败
		if(LOBYTE(WsaData.wVersion)!=2||HIBYTE(WsaData.wHighVersion)!=2) {
			WSACleanup();
			return 0;
		}
	}
#endif

	return 1;
}

/*
函数原型：int UDP_Open(char* chLocalIP, unsigned short& nLocalPort, bool bNonBlock)
参数	：
		chLocalIP[in]		打开端口时绑定的本机IP地址
		nLocalPort[inout]	传入时，表示指定要打开的端口号，为0时表示要系统随机分配一个空闲端口;传出时，实际打开的端口号，主要是应用于传入0时，返回系统分配的端口号
		bNonBlock[in]		是否为阻塞方式，true非阻塞方式， false 阻塞方式
返回值：1：打开端口成功；0：打开打开端口失败
函数功能：打开指定端口
*/
int UDP_Open(const char* chLocalIP, unsigned short& nLocalPort, bool bNonBlock)
{
	//socket变量
	//SOCKET newSocket = 0;
	int newSocket = 0;

	int reuseFlag0 = 1;
	struct sockaddr_in server;
	int len =sizeof(server);
	server.sin_family=AF_INET;
	//本地端口
	server.sin_port=htons((unsigned short)(nLocalPort));
	//本地IP地址
	if(chLocalIP == NULL || (strcmp(chLocalIP,"")==0) || (strcmp(chLocalIP,"127.0.0.1")==0))
		server.sin_addr.s_addr=htonl(0);
	else
		server.sin_addr.s_addr=inet_addr(chLocalIP);
	memset(&(server.sin_zero),0,8);

	//UDP协议 - 参数3: 0-自动选择协议
	newSocket=socket(AF_INET,SOCK_DGRAM, IPPROTO_UDP );

	// 设置地址重用
	if (setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseFlag0, sizeof reuseFlag0) < 0)
	{
		//closeSocket(newSocket);
		close(newSocket);

		return -1;
	}
	//绑定端口
    if (bind(newSocket,(struct sockaddr*)&server,sizeof(struct sockaddr)) == -1)
	{
		//绑定端口失败，关闭socket
		//closesocket(newSocket);
    	close(newSocket);
		//返回失败
		return (0);
	}
	//设置阻塞或非阻塞方式
	if(bNonBlock == true)
	{
		//将套接字设置为非阻塞方式
		makeSocketNonBlocking(newSocket);
	}
	else
	{
		//将套接字设置为阻塞方式
		makeSocketBlocking(newSocket);
	}
	//得到端口号
	len = sizeof(server);
    if(getsockname(newSocket, (struct sockaddr *)&server, (unsigned int *)&len) != 0)
	{
		//关闭socket
        //closesocket(newSocket);
    	close(newSocket);
        return  (0);
    }
	nLocalPort = ntohs(server.sin_port);

	//返回socket
	return newSocket;
}
/*
函数原型：void UDP_SetRecvBufferSize(int sock, int nSize)
参数	：
sock[in]		socket连接
nSize[in]		接收数据缓冲区大小
函数功能：设置套接字接收缓冲区大小
*/
void UDP_SetRecvBufferSize(const int sock, const int nSize)
{
	int iSetCount = 0;
	int iOptVal = 435;
	int iOptLen = sizeof(int);
	//接收缓冲区大小
	int iRecvBufSize = nSize;

	//设置接收缓冲区大小
	do
	{
		//设置接受缓冲区长度
		//if(setsockopt(sock, SOL_SOCKET, SO_RCVBUF,(const char*)&iRecvBufSize, sizeof iRecvBufSize) == SOCKET_ERROR)
		if(setsockopt(sock, SOL_SOCKET, SO_RCVBUF,(const char*)&iRecvBufSize, sizeof iRecvBufSize) < 0)
			return;
		//获取接受缓冲区长度
		//if(getsockopt(sock, SOL_SOCKET, SO_RCVBUF,(void*)&iOptVal, (unsigned int *)&iOptLen) == SOCKET_ERROR)
		if(getsockopt(sock, SOL_SOCKET, SO_RCVBUF,(void*)&iOptVal, (unsigned int *)&iOptLen) < 0 )
			return;
		//比较设置的缓冲区长度是否成功
		if(iOptVal == iRecvBufSize)
			return;
		//获取的长度和设置的长度不一样 取两个的中间值 重新设置一次
		iRecvBufSize = (iRecvBufSize + iOptVal)/2;
		//设置次数加1 最多设置5次
		iSetCount = iSetCount +1;
	}while(iSetCount < 5);
	return;
}
/*
函数原型：void UDP_SetSendBufferSize(int sock, int nSize)
参数	：
sock[in]		socket连接
nSize[in]		接收数据缓冲区大小
函数功能：设置套接字接收缓冲区大小
*/
void UDP_SetSendBufferSize(const int sock, const int nSize)
{
	int iSetCount = 0;
	int iOptVal;
	int iOptLen = sizeof(int);
	//发送缓冲区大小
	int iSendBufSize = nSize;
	//设置发送缓冲区大小
	do
	{
		//设置接受缓冲区长度
		//if(setsockopt(sock, SOL_SOCKET, SO_SNDBUF,(const char*)&iSendBufSize, sizeof iSendBufSize) == SOCKET_ERROR)
		if(setsockopt(sock, SOL_SOCKET, SO_SNDBUF,(const char*)&iSendBufSize, sizeof iSendBufSize) < 0 )
			return;
		//获取接受缓冲区长度
		//if(getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&iOptVal, (unsigned int *)&iOptLen) == SOCKET_ERROR)
		if(getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&iOptVal, (unsigned int *)&iOptLen) <0)
			return;
		//比较设置的缓冲区长度是否成功
		if(iOptVal == iSendBufSize)
			return;
		//获取的长度和设置的长度不一样 取两个的中间值 重新设置一次
		iSendBufSize = (iSendBufSize + iOptVal)/2;
		//设置次数加1 最多设置5次
		iSetCount = iSetCount +1;
	}while(iSetCount < 5);

	return;
}

/*
函数原型：int UDP_Send(int	sock,char* RemoteIP,int RemotePort,char* Buf, int Len)
参数	：
		sock[in]		socket连接
		RemoteIP[in]	目标IP地址
		RemotePort[in]	目标端口号
		Buf[int]		要发送的数据缓冲区
		Len[int]		要发送的数据长度
返回值：实际发送的数据长度
函数功能：发送数据到指定地址的指定端口
*/
//int UDP_Send(const int sock, unsigned long nRemoteIP,unsigned short nRemotePort,const char* pData, const int nLen)
int UDP_Send(const int sock, unsigned long nRemoteIP,unsigned short nRemotePort,const char* pData, const int nLen)
{
	fd_set fdr;
	//成功发送数据的长度
	int sendLen = 0;
	//发送超时时间
	struct timeval timeout = {0,2000};
	//判断是否有数据要接收
	FD_ZERO(&fdr);
	FD_SET(sock, &fdr);
	select(sock + 1, NULL, &fdr, NULL, &timeout);
	//如果有数据要接收才调用recvfrom函数接收数据
	if(FD_ISSET(sock,&fdr))
	{
		//发送的目标地址
		struct sockaddr_in RemoteAddr;
		int addrLen =sizeof(RemoteAddr);
		//组织目标地址信息
		RemoteAddr.sin_family = AF_INET;

		//目标IP地址
		//RemoteAddr.sin_addr.S_un.S_addr = nRemoteIP;
		RemoteAddr.sin_addr.s_addr = nRemoteIP;

		memset(&(RemoteAddr.sin_zero),0,8);
		//目标端口
		RemoteAddr.sin_port = htons((unsigned short)(nRemotePort));

		//发送网络数据包
		sendLen = sendto(sock, pData, nLen,0,(struct sockaddr*)&RemoteAddr,addrLen);
	}
	FD_CLR(sock, &fdr);
	//FD_CLR((unsigned int)sock, &fdr);
	//返回发送的数据长度
	return(sendLen);
}
/*
函数原型：int UDP_Recv(int	sock, unsigned long &nFromIP, unsigned short &nFromPort,char* Buf, int Len, unsigned int time)
参数	：
		sock[in]		socket连接
		FromIP[out]		数据发送方IP地址
		FromPort[out]	数据发送方端口
		Buf[out]		接收数据的缓冲区
		Len[in]			接收数据缓冲区的长度
返回值：接收的数据长度
函数功能：接收数据
*/
int UDP_Recv(const int sock, unsigned long &nFromIP, unsigned short &nFromPort, char* pData, const int Len, unsigned int time)
{
	//接收到数据的长度
	int recvLen = 0;
	fd_set fdr;
	//接收超时时间
	struct timeval timeout = {0,time};
	//判断是否有数据要接收
	FD_ZERO(&fdr);
	FD_SET(sock, &fdr);
	//如果有数据要接收才调用recvfrom函数接收数据
	select(sock + 1, &fdr, NULL,NULL, &timeout);
	if(FD_ISSET(sock,&fdr))
	{
		//数据的源地址
		struct sockaddr_in fromAddr;
		int addrLen = 0;
		//接收网络数据
		addrLen = sizeof(fromAddr);
		recvLen = recvfrom(sock,	// socket
			pData,							// recieve buffer
			Len,							// size of recieve buffer
			0,								// flag
			(struct sockaddr *)&fromAddr,	// from
			(unsigned int *)&addrLen);
		//recvLen大于0，表示接收到了数据
		if(recvLen > 0)
		{
			//得到数据发送端的IP地址
			nFromIP = fromAddr.sin_addr.s_addr;
			//得到数据发送端的端口号
			nFromPort = ntohs(fromAddr.sin_port);
		}
	}
	FD_CLR(sock, &fdr);
	return(recvLen);
}
/*
函数原型：int UDP_Close(int sock)
参数	：
		sock[in]		socket连接
返回值：1：关闭socket连接成功；0：关闭socket连接失败。
函数功能：关闭指定socket连接
*/
int UDP_Close(int sock)
{
	//如果sock大于0，表示处于打开状态
	if(sock <= 0)
	{
		return 1;
	}
	//关闭socket失败
	if(close(sock) != 0)
	{
		return 0;
	}
	//关闭成功
	return 1;
}
/*
函数原型：int UDP_Free();
入口参数：无；
出口参数：无；
返回值：	1：成功；0：失败；
函数功能：终止网络的使用，这个函数和UDP_Init对应的，调用此函数前必须调用UDP_Init函数，否则可能会引起其他网络模块的不正常。
*/
int UDP_Free()
{
	return 1;

//	//成功时返回值为0
//	if(WSACleanup() == 0)
//	{
//		return (1);
//	}
//	//返回失败
//	return (0);
}
