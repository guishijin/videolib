/*
 * CommTCP.h
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */

#ifndef COMMTCP_H_
#define COMMTCP_H_

//连接回调函数
//typedef void (FAR __stdcall *CallbackConnectFuc)(void* pObj,int sock,char *ServerIP, int nServerPort,char *ClientIP, unsigned short nClientPort);
typedef void ( *CallbackConnectFuc)(void* pObj,int sock,char *ServerIP, int nServerPort,char *ClientIP, unsigned short nClientPort);

/*
函数原型：int TCP_Init()
参数	：无
返回值：1：初始化成功；0：初始化失败
函数功能：初始化，使用时要先调用初始化函数
*/
int TCP_Init();

/*
函数原型：int TCP_StartServer(void* pObj,char* LocalIP, int* nPort,int nMaxClient,CallbackConnectFuc ConnectFuc)
参数	：	LocallIP[in]	绑定的本机IP地址，如"172.16.2.11"。
			nPort[inout]	传入时，表示指定要打开的端口号，为0时表示要系统随机分配一个空闲端口;传出时，实际打开的端口号，主要是应用于传入0时，返回系统分配的端口号
			nMaxClient[in]	允许的最大连接数。
			ConnectFuc[in]	有客户端连接时的回调函数
返回值：	0：创建服务器失败；非0时为创建TCP服务器返回的socket套接字。
函数功能：创建并启动一个TCP服务器监听指定端口或随机系统分配端口，有客户端连接时调用回调函数通知应用程序。
*/
int TCP_StartServer(void* pObj,char* LocalIP, unsigned short* nPort,int nMaxClient,CallbackConnectFuc ConnectFuc);

/*
函数原型：int TCP_Connect(int *nLocalPort,char* chServerIP, unsigned short nServerPort)
参数	：	nLocalPort[in]	本机IP地址，如"172.16.2.11"。
			chServerIP[in]	服务器IP地址
			nServerPort[in]	服务器端口号
返回值：	0：连接服务器失败；非0时为连接TCP服务器的socket套接字。
函数功能：创建并启动一个TCP服务器监听指定端口或随机系统分配端口，有客户端连接时调用回调函数通知应用程序。
*/
int TCP_Connect(int *nLocalPort,char* chServerIP, unsigned short nServerPort);
/*
函数原型：void TCP_SetRecvBufferSize(int sock, int nSize)
参数	：
sock[in]		socket连接
nSize[in]		接收数据缓冲区大小
函数功能：设置套接字接收缓冲区大小
*/
void TCP_SetRecvBufferSize(const int sock, const int nSize);
/*
函数原型：void TCP_SetSendBufferSize(int sock, int nSize)
参数	：
sock[in]		socket连接
nSize[in]		接收数据缓冲区大小
函数功能：设置套接字接收缓冲区大小
*/
void TCP_SetSendBufferSize(const int sock, const int nSize);
/*
函数原型：int TCP_Recv(int	sock,char* Buf, int* Len)
参数	：
sock[in]		socket连接
Buf[int]		接收数据缓冲区
Len[int]		接收数据缓冲区长度
返回值：  接收到的数据长度
函数功能：接收指定套接字的网络数据
*/
int TCP_Recv(const int sock,char* pData,const int nLen,const unsigned int nTime);

/*
函数原型：int TCP_Send(int	sock,char* Buf, int Len)
参数	：
sock[in]		socket连接
Buf[int]		要发送的数据缓冲区
Len[int]		要发送的数据长度
返回值：实际发送的数据长度
函数功能：发送数据到指定地址的指定端口
*/
int TCP_Send(const int sock,const char* Buf,const int Len);

/*
函数原型：int TCP_Close(int* sock)
参数	：
sock[in]		socket连接
返回值：1：关闭socket连接成功；0：关闭socket连接失败。
函数功能：关闭指定socket连接
*/
int TCP_Close(int sock);

/*
函数原型：int UDP_Free();
入口参数：无；
出口参数：无；
返回值：	1：成功；0：失败；
函数功能：终止网络的使用，这个函数和UDP_Init对应的，调用此函数前必须调用UDP_Init函数，否则可能会引起其他网络模块的不正常。
*/
int TCP_Free();



#endif /* COMMTCP_H_ */
