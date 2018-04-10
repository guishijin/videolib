/*
 * CommTCP.cpp
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */


#include "CommTCP.h"
#include "CMyPing.h"

#include <arpa/inet.h>

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <pthread.h>

//定义结构及宏
typedef struct
{
	u_long	onoff;
	u_long	keepalivetime;
	u_long	keepaliveinterval;
}tcp_keepalive;
//#define SIO_KEEPALIVE_VALS _WSAIOW(IOC_VENDOR,4)

//服务器信息结构
typedef struct
{
	char strIP[20];
	unsigned short nPort;
	//SOCKET   sock;
	int sock;
	void* pObj;
	CallbackConnectFuc Fuc;
}ServerInfo;

//服务器监听线程
//unsigned int ThreadLisen(void* pParam)
void* ThreadLisen(void* pParam)
{
	ServerInfo* pServer = (ServerInfo*)pParam;
	CallbackConnectFuc pConnectFuc;
	int sock;
	//TCP服务器套接字
	sock = pServer->sock;
	//连接回调函数
	pConnectFuc		= pServer->Fuc;
	//接受请求后，实际同客户端socket进行交互的SOCKET client
	int Client;
	sockaddr_in from;
	int fromlen=sizeof(from);
	//线程主循环，监听新连接
	while(true)
	{
		//接受一个连接请求
		Client=accept(sock,(struct sockaddr*)&from,(unsigned int *)&fromlen);
		//有客户端请求连接
		if(Client != -1)
		{
			static int reuseFlag = 256*1024;
			//KeepAlive实现
			tcp_keepalive inKeepAlive = {0}; //输入参数
			unsigned long ulInLen = sizeof(tcp_keepalive);

			tcp_keepalive outKeepAlive = {0}; //输出参数
			unsigned long ulOutLen = sizeof(tcp_keepalive);

			unsigned long ulBytesReturn = 0;

			//设置socket的keep alive为5秒，并且发送次数为3次
			inKeepAlive.onoff = 1;
			inKeepAlive.keepaliveinterval = 5000;	//两次KeepAlive探测间的时间间隔
			inKeepAlive.keepalivetime = 5000;		//开始首次KeepAlive探测前的TCP空闭时间

//			if (WSAIoctl((unsigned int)Client, SIO_KEEPALIVE_VALS,
//				(LPVOID)&inKeepAlive, ulInLen,
//				(LPVOID)&outKeepAlive, ulOutLen,
//				&ulBytesReturn, NULL, NULL) == SOCKET_ERROR)
//			{
//				//关闭Client
//				//closesocket(Client);
//				close(Client);
//				Client=0;
//				continue;
//			}
			// 替换实现
//			int keepalive = 1; // 开启keepalive属性
//			int keepidle = 60; // 如该连接在60秒内没有任何数据往来,则进行探测
//			int keepinterval = 5; // 探测时发包的时间间隔为5 秒
//			int keepcount = 3; // 探测尝试的次数.如果第1次探测包就收到响应了,则后2次的不再发.
//			setsockopt(Client, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepalive , sizeof(keepalive ));
//			setsockopt(Client, SOL_TCP, TCP_KEEPIDLE, (void*)&keepidle , sizeof(keepidle ));
//			setsockopt(Client, SOL_TCP, TCP_KEEPINTVL, (void *)&keepinterval , sizeof(keepinterval ));
//			setsockopt(Client, SOL_TCP, TCP_KEEPCNT, (void *)&keepcount , sizeof(keepcount ));

			//Specify the total per-socket buffer space reserved for receives.
			if (setsockopt(Client, SOL_SOCKET, SO_RCVBUF,(const char*)&reuseFlag, sizeof reuseFlag) < 0)
			{
				//关闭Client
				//closesocket(Client);
				close(Client);
				Client=0;
				continue;
			}
			//回调函数
			if(pConnectFuc != NULL)
			{
				pConnectFuc(pServer->pObj,(int)Client,pServer->strIP,pServer->nPort,inet_ntoa(from.sin_addr),ntohs(from.sin_port));
			}
		}
		else if(Client == -1)
		{
			break;
		}
	}
	//删除服务器信息
	delete pServer;
	pServer=NULL;
	return 0;
}

/*
函数原型：int TCP_Init()
参数	：无
返回值：1：初始化成功；0：初始化失败
函数功能：初始化，使用时要先调用初始化函数
*/
int TCP_Init()
{
#if defined(__WIN32__) || defined(_WIN32)
	WORD VersionRequested;
	WSADATA WsaData;
	//socket版本为2.2版
	VersionRequested=MAKEWORD(2,2);
	if(WSAStartup(VersionRequested,&WsaData) !=0 )
	{
		return 0;
	}
	else{
		//版本不对，返回失败
		if(LOBYTE(WsaData.wVersion)!=2||HIBYTE(WsaData.wHighVersion)!=2)
		{
			WSACleanup();
			return 0;
		}
	}
#endif

	return 1;
}

/*
函数原型：int TCP_StartServer(void* pObj,char* LocalIP, int* nPort,int nMaxClient,CallbackConnectFuc ConnectFuc)
参数	：	LocallIP[in]	绑定的本机IP地址，如"172.16.2.11"。
			nPort[inout]	传入时，表示指定要打开的端口号，为0时表示要系统随机分配一个空闲端口;传出时，实际打开的端口号，主要是应用于传入0时，返回系统分配的端口号
			nMaxClient[in]	允许的最大连接数。
			ConnectFuc[in]	有客户端连接时的回调函数
返回值：	0：创建服务器失败；非0时为创建TCP服务器返回的socket套接字。
函数功能：创建并启动一个TCP服务器监听指定端口或随机系统分配端口，有客户端连接时调用回调函数通知应用程序。
*/
int TCP_StartServer(void* pObj,char* LocalIP, unsigned short* nPort,int nMaxClient,CallbackConnectFuc ConnectFuc)
{
	//服务器信息结构，作为线程参数，启动监听线程
	ServerInfo* Info = new ServerInfo;
	struct sockaddr_in server;
	int len =sizeof(server);
	server.sin_family=AF_INET;
	//本地端口
	server.sin_port=htons((unsigned short)(*nPort));
	//本地IP地址
	if(LocalIP == NULL || (strcmp(LocalIP,"")==0) || (strcmp(LocalIP,"127.0.0.1")==0))
	{
		server.sin_addr.s_addr=INADDR_ANY;
	}
	else
	{
		server.sin_addr.s_addr=inet_addr(LocalIP);
	}
	memset(&(server.sin_zero),0,8);
	//建立一个tcp socket
	Info->sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	//判断tcp socket是否创建成功
	if(Info->sock==-1)
	{
		delete Info;Info = NULL;
		return (0);
	}
	//绑定服务器的ip地址和端口号
	if(bind(Info->sock,(sockaddr*)&server,sizeof(server))!=0)
	{
		//关闭socket
		//closesocket(Info->sock);
		close(Info->sock);
		Info->sock=NULL;
		delete Info;
		Info = NULL;
		return (0);
	}
	//监听客户端请求，最大连接数
	if(listen(Info->sock,nMaxClient)!=0)
	{
		//关闭socket
		//closesocket(Info->sock);
		close(Info->sock);
		Info->sock=NULL;
		delete Info;Info = NULL;
		return (0);
	}
	//得到端口号
	len = sizeof(server);
	if(getsockname(Info->sock, (struct sockaddr *)&server, (unsigned int *)&len) != 0)
	{
		//关闭socket
		//closesocket(Info->sock);
		close(Info->sock);
		Info->sock=NULL;
		delete Info;Info = NULL;
		return  (0);
	}
	*nPort = ntohs(server.sin_port);
	//本地IP地址
	sprintf(Info->strIP,"%s\0",LocalIP);
	//监听端口
	Info->nPort = *nPort;
	//创建的socket套接字
	//Info->sock  = Info->sock;

	//调用对象
	Info->pObj  = pObj;
	//连接回调函数
	Info->Fuc = ConnectFuc;
	//启动监听线程
	//AfxBeginThread(ThreadLisen,Info);
	pthread_t serverthread;
	pthread_create(&serverthread, NULL, ThreadLisen, Info );

	return Info->sock;
}
/*
函数原型：int TCP_Connect(int *nLocalPort,char* chServerIP, unsigned short nServerPort)
参数	：	nLocalPort[in]	本机IP地址，如"172.16.2.11"。
			chServerIP[in]	服务器IP地址
			nServerPort[in]	服务器端口号
返回值：	0：连接服务器失败；非0时为连接TCP服务器的socket套接字。
函数功能：创建并启动一个TCP服务器监听指定端口或随机系统分配端口，有客户端连接时调用回调函数通知应用程序。
*/
int TCP_Connect(int *nLocalPort,char* chServerIP, unsigned short nServerPort)
{
	static int reuseFlag = 256*1024;

	int reuseFlag0 = 0;
	//KeepAlive参数
	tcp_keepalive inKeepAlive = {0}; //输入参数
	unsigned long ulInLen = sizeof(tcp_keepalive);

	tcp_keepalive outKeepAlive = {0}; //输出参数
	unsigned long ulOutLen = sizeof(tcp_keepalive);

	unsigned long ulBytesReturn = 0;
	//服务器信息
	struct sockaddr_in server;
	int len =sizeof(server);
	////////////////测试ping命令/////////////////////
	CMyPing* MyPing = new CMyPing();
	if(true == MyPing->Init())
	{
		if(0 == MyPing->SendPing(chServerIP,500,2))
		{
			delete MyPing;MyPing = NULL;
			return (0);
		}
	}
	delete MyPing;MyPing = NULL;
	////////////////测试ping命令 END/////////////////
	server.sin_family=AF_INET;
	//本地端口
	server.sin_port=htons((unsigned short)(nServerPort));
	//本地IP地址
	server.sin_addr.s_addr=inet_addr(chServerIP);
	memset(&(server.sin_zero),0,8);
	//建立一个tcp socket
	int newSocket=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	//判断tcp socket是否创建成功
	if(newSocket == -1)
	{
		return (0);
	}

//	//设置socket的keep alive为5秒，并且发送次数为3次
//	inKeepAlive.onoff = 1;
//	inKeepAlive.keepaliveinterval = 5000;	//两次KeepAlive探测间的时间间隔
//	inKeepAlive.keepalivetime = 5000;		//开始首次KeepAlive探测前的TCP空闭时间
//
//	if (WSAIoctl((unsigned int)newSocket, SIO_KEEPALIVE_VALS,
//		(LPVOID)&inKeepAlive, ulInLen,
//		(LPVOID)&outKeepAlive, ulOutLen,
//		&ulBytesReturn, NULL, NULL) == SOCKET_ERROR)
//	{
//		//关闭Client
//		closesocket(newSocket);newSocket=0;
//		return(0);
//	}

	//
	if (setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseFlag0, sizeof reuseFlag0) < 0)
	{
		close(newSocket);
		return -1;
	}
	//不等于0时，表示连接服务器失败
	if( connect( newSocket,(const struct sockaddr *)&server,sizeof( struct sockaddr) ) != 0 )
	{
		//关闭socket
		close(newSocket);
		newSocket=0;
		return(0);
	}
	//得到端口号
	if(getsockname(newSocket, (struct sockaddr *)&server, (unsigned int *)&len) != 0)
	{
		//关闭socket
		close(newSocket);
		newSocket=0;
		return  (0);
	}
	//得到创建socket套接字使用的本地端口
	*nLocalPort = ntohs(server.sin_port);
	//本地IP地址
	return (int)newSocket;
}
/*
函数原型：void TCP_SetRecvBufferSize(int sock, int nSize)
参数	：
sock[in]		socket连接
nSize[in]		接收数据缓冲区大小
函数功能：设置套接字接收缓冲区大小
*/
void TCP_SetRecvBufferSize(const int sock, const int nSize)
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
		//if(setsockopt((SOCKET)sock, SOL_SOCKET, SO_RCVBUF,(const char*)&iRecvBufSize, sizeof iRecvBufSize) == SOCKET_ERROR)
		if(setsockopt(sock, SOL_SOCKET, SO_RCVBUF,(const char*)&iRecvBufSize, sizeof iRecvBufSize) < 0 )
			return;
		//获取接受缓冲区长度
		//if(getsockopt((SOCKET)sock, SOL_SOCKET, SO_RCVBUF, (char*)&iOptVal, &iOptLen) == SOCKET_ERROR)
		if(getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&iOptVal, (unsigned int *)&iOptLen) < 0 )
			return;
		//比较设置的缓冲区长度是否成功
		if(iOptVal == iRecvBufSize)
			return;
		//获取的长度和设置的长度不一样 取两个的中间值 重新设置一次
		iRecvBufSize = (iRecvBufSize + iOptVal)/2;
		//设置次数加1 最多设置5次
		iSetCount = iSetCount +1;
	}
	while(iSetCount < 5);
}
/*
函数原型：void TCP_SetSendBufferSize(int sock, int nSize)
参数	：
sock[in]		socket连接
nSize[in]		接收数据缓冲区大小
函数功能：设置套接字接收缓冲区大小
*/
void TCP_SetSendBufferSize(const int sock, const int nSize)
{
	int iSetCount = 0;
	int iOptVal;
	int iOptLen = sizeof(int);
	//发送缓冲区大小
	int iSendBufSize = nSize;
	//设置发送缓冲区大小
	do{
		//设置接受缓冲区长度
		//if(setsockopt(sock, SOL_SOCKET, SO_SNDBUF,(const char*)&iSendBufSize, sizeof iSendBufSize) == SOCKET_ERROR)
		if(setsockopt(sock, SOL_SOCKET, SO_SNDBUF,(const char*)&iSendBufSize, sizeof iSendBufSize) < 0 )
			return;
		//获取接受缓冲区长度
		//if(getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&iOptVal, (unsigned int*)&iOptLen) == SOCKET_ERROR)
		if(getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&iOptVal, (unsigned int*)&iOptLen) <0)
			return;
		//比较设置的缓冲区长度是否成功
		if(iOptVal == iSendBufSize)
			return;
		//获取的长度和设置的长度不一样 取两个的中间值 重新设置一次
		iSendBufSize = (iSendBufSize + iOptVal)/2;
		//设置次数加1 最多设置5次
		iSetCount = iSetCount +1;
	}while(iSetCount < 5);
}
/*
函数原型：int TCP_Recv(int	sock,char* Buf, int* Len)
参数	：
sock[in]		socket连接
Buf[int]		接收数据缓冲区
Len[int]		接收数据缓冲区长度
返回值：  接收到的数据长度
函数功能：接收指定套接字的网络数据
*/
int TCP_Recv(const int sock,char* pData,const int nLen,const unsigned int nTime)
{
	fd_set fdr;
	//接收到数据的长度
	int readLen = 0;
	//接收超时时间
	struct timeval timeout = {0,nTime};
	//判断是否有数据要接收
	FD_ZERO(&fdr);
	FD_SET((unsigned int)sock, &fdr);

	//如果有数据要接收才调用recvfrom函数接收数据
	int retselect = select(sock + 1, &fdr, NULL, NULL, &timeout);

	// 0-select超时，-1-select发生错误，>0有事件产生
	if(retselect > 0)
	{
		if(FD_ISSET(sock,&fdr))
		{
			//发送网络数据包
			readLen = recv(sock,(char* )pData, nLen,0);
			//断开连接
			if(readLen == 0)
			{
				//return (-1);
				readLen = -1;
			}
		}
	}
	else if(retselect == 0)
	{
		// 超时发生，返回0
		readLen = 0;
	}
	else
	{
		// 其他错误，返回-1
		readLen = -1;
	}

	FD_CLR((unsigned int)sock, &fdr);

	//返回接收到的数据长度
	return(readLen);
}

/*
函数原型：int TCP_Send(int	sock,char* Buf, int Len)
参数	：
sock[in]		socket连接
Buf[int]		要发送的数据缓冲区
Len[int]		要发送的数据长度
返回值：实际发送的数据长度
函数功能：发送数据到指定地址的指定端口
*/
int TCP_Send(const int sock,const char* Buf,const int Len)
{
	fd_set fdr;
	//成功发送数据的长度
	int sendLen = 0;
	//接收超时时间
	struct timeval timeout = {0,2000};
	//判断是否有数据要接收
	FD_ZERO(&fdr);
	FD_SET((unsigned int)sock, &fdr);
	//发送数据
	select(sock + 1, NULL, &fdr, NULL, &timeout);
	if(FD_ISSET(sock,&fdr))
	{
		//发送网络数据包
		sendLen = send(sock,Buf,Len,0);
	}
	FD_CLR((unsigned int)sock, &fdr);
	//返回发送的数据长度
	return(sendLen);
}
/*
函数原型：int TCP_Close(int* sock)
参数	：
sock[in]		socket连接
返回值：1：关闭socket连接成功；0：关闭socket连接失败。
函数功能：关闭指定socket连接
*/
int TCP_Close(int sock)
{
	//如果sock小于等于0，表示处于未打开状态
	if(sock <= 0)
	{
		return 1;
	}
	//关闭socket失败
	if(close(sock) != 0)
	{
		return 0;
	}
	return 1;
}
/*
函数原型：int UDP_Free();
入口参数：无；
出口参数：无；
返回值：	1：成功；0：失败；
函数功能：终止网络的使用，这个函数和UDP_Init对应的，调用此函数前必须调用UDP_Init函数，否则可能会引起其他网络模块的不正常。
*/
int TCP_Free()
{
#if defined(__WIN32__) || defined(_WIN32)
	//成功时返回值为0
	if(WSACleanup() == 0)
	{
		return (1);
	}
#else
	return 1;
#endif

	//返回失败
	return (0);
}



