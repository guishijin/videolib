/*
 * CommUDP.h
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */

#ifndef COMMUDP_H_
#define COMMUDP_H_



/*
函数原型：int UDP_Init()
参数	：无
返回值：1：初始化成功；0：初始化失败
函数功能：初始化，使用时要先调用初始化函数
*/
/**
 * 初始化
 * @return
 */
int UDP_Init();

/*
函数原型：int UDP_Open(char* chLocalIP, unsigned short& nLocalPort, bool bNonBlock)
参数	：
		chLocalIP[in]		打开端口时绑定的本机IP地址
		nLocalPort[inout]	传入时，表示指定要打开的端口号，为0时表示要系统随机分配一个空闲端口;传出时，实际打开的端口号，主要是应用于传入0时，返回系统分配的端口号
		bNonBlock[in]		是否为阻塞方式，true非阻塞方式， false 阻塞方式
返回值：1：打开端口成功；0：打开打开端口失败
函数功能：打开指定端口
*/
/**
 * 打开
 * @param chLocalIP
 * @param nLocalPort
 * @param bNonBlock
 * @return
 */
int UDP_Open(const char* chLocalIP, unsigned short& nLocalPort, bool bNonBlock);

/*
函数原型：void UDP_SetRecvBufferSize(int sock, int nSize)
参数	：
sock[in]		socket连接
nSize[in]		接收数据缓冲区大小
函数功能：设置套接字接收缓冲区大小
*/
/**
 * 设置接收缓冲区大小
 * @param sock
 * @param nSize
 */
void UDP_SetRecvBufferSize(const int sock, const int nSize);

/*
函数原型：void UDP_SetSendBufferSize(int sock, int nSize)
参数	：
sock[in]		socket连接
nSize[in]		接收数据缓冲区大小
函数功能：设置套接字接收缓冲区大小
*/
/**
 * 设置发送缓冲区大小
 * @param sock
 * @param nSize
 */
void UDP_SetSendBufferSize(const int sock, const int nSize);

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
/**
 * 发送数据
 * @param sock
 * @param nRemoteIP
 * @param nRemotePort
 * @param pData
 * @param nLen
 * @return
 */
int UDP_Send(const int sock, unsigned long nRemoteIP,unsigned short nRemotePort,const char* pData, const int nLen);

/*
函数原型：int UDP_Recv(int	sock,unsigned long &nFromIP, unsigned short &nFromPort,char* Buf, int Len, unsigned int time)
参数	：
		sock[in]		socket连接
		FromIP[out]		数据发送方IP地址
		FromPort[out]	数据发送方端口
		Buf[out]		接收数据的缓冲区
		Len[in]			接收数据缓冲区的长度
返回值：接收的数据长度
函数功能：接收数据
*/
/**
 * 接收数据
 * @param sock
 * @param nFromIP
 * @param nFromPort
 * @param pData
 * @param Len
 * @param time
 * @return
 */
int UDP_Recv(const int sock, unsigned long &nFromIP, unsigned short &nFromPort, char* pData, const int Len, const unsigned int time);

/*
函数原型：int UDP_Close(int sock)
参数	：
		sock[in]		socket连接
返回值：1：关闭socket连接成功；0：关闭socket连接失败。
函数功能：关闭指定socket连接
*/
/**
 * 关闭
 * @param sock
 * @return
 */
int UDP_Close(int sock);

/*
函数原型：int UDP_Free();
入口参数：无；
出口参数：无；
返回值：	1：成功；0：失败；
函数功能：终止网络的使用，这个函数和UDP_Init对应的，调用此函数前必须调用UDP_Init函数，否则可能会引起其他网络模块的不正常。
*/
/**
 * 释放
 * @return
 */
int UDP_Free();


#endif /* COMMUDP_H_ */
