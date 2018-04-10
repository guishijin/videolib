/*
 * CRtspServer.cpp
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */




#include "CommTCP.h"
#include "CommUDP.h"
#include "CRtspServer.h"

#include <arpa/inet.h>

#include "CRtspClient.h"


#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// FIXME： windows的特有宏
#define WM_USER 1024
//RTSP参数的最大长度
#define RTSP_PARAM_STRING_MAX	200
//客户端连接
#define WM_CLIENT_ADD	(WM_USER + 2001)

//服务程序及版本
static const char* libNameStr= "P2PServer";
static const char* libVersionStr = "(2017-08-08)";

//从视频流中获取IP地址和端口
bool anlayConStr(int8* chStreamURL,int8* IP,int32* port);

//RTSP允许的操作命令
//static char const* g_AllowedCommandNames = "OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY,";
static char const* g_AllowedCommandNames = "OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, SET_PARAMETER";

//通讯检测数据包
char HeartBuf[4] = {0x00, 0x00, 0x00, 0x00};

//解析RTSP请求命令
extern bool parseRTSPRequestString(char const* reqStr,
							unsigned int reqStrSize,
							char* resultCmdName,
							unsigned int resultCmdNameMaxSize,
							char* resultURLPreSuffix,
							unsigned int resultURLPreSuffixMaxSize,
							char* resultURLSuffix,
							unsigned int resultURLSuffixMaxSize,
							char* resultCSeq,
							unsigned int resultCSeqMaxSize,
							char* resultSessionIdStr,
							unsigned int resultSessionIdStrMaxSize,
							unsigned int& contentLength);
//初始化RTP通讯
extern bool InitiateRTPandRTCP(bool bMultiplexRTCPWithRTP, int32& rtpSock, int32& rtcpSock, char const * localAddr, uint16& rtpPortNum, uint16& rtcpPortNum, bool bNonBlock);
//释放RTP通讯
extern void ReleaseRTPandRTCP(int32& rtpSock, int32& rtcpSock);

// 生成随机数
unsigned long our_random32()
{
	long random_1 = rand();
	unsigned long random16_1 = (unsigned long)(random_1&0x00FFFF00);

	long random_2 = rand();
	unsigned long random16_2 = (unsigned long)(random_2&0x00FFFF00);

	return (random16_1<<8) | (random16_2>>8);
}

/**
 * TCP连接回调函数
 *      所有的客户端链接信息经过此函数添加到RTSP服务器中。
 */
void DllConnectFuc(void* pObj,int nClientSock, int8 * chServerIP, int nServerPort, int8 * chClientIP, uint16 nClientPort)
{
	// 服务器实例指针
	CRtspServer* pServer = (CRtspServer*)pObj;

	//判断服务器地址
	if(strcmp(pServer->m_chServerIP,chServerIP) != 0)
	{
		TCP_Close(nClientSock);
		nClientSock = -1;
		printf("新客户端链接失败[line：84]： [%s:%d]\r\n", chClientIP, nClientPort );
		return;
	}

	//判断服务器端口
	if(pServer->m_nServerPort != nServerPort)
	{
		TCP_Close(nClientSock);
		nClientSock = -1;
		printf("新客户端链接失败[line：94]： [%s:%d]\r\n", chClientIP, nClientPort );
		return;
	}

	printf("现在登记：  新客户端链接......[%s:%d]\r\n", chClientIP, nClientPort );

	//将客户端信息添加到列表
	if(pServer->AddRtspClient(nClientSock, chClientIP, nClientPort))
	{
		printf("新客户端链接成功： [%s:%d]\r\n", chClientIP, nClientPort );

		return;
	}

	printf("新客户端链接失败[line：105]： [%s:%d]\r\n", chClientIP, nClientPort );
	TCP_Close(nClientSock);
	nClientSock = -1;
	return;
}

/**
 * 构造函数
 * @param chServerIP 服务器绑定ip
 * @param nServerPort 服务器绑定端口
 * @param nThreadID 外部服务程序的线程ID
 * @param nMsgID 外部服务程序的线程检索的消息ID
 */
CRtspServer::CRtspServer(int8* chServerIP, uint16 nServerPort, DWORD nThreadID,DWORD nMsgID)
{
	//初始化类成员变量
	//通讯处理线程句柄,线程启动时获得
	m_hCommDealThread = 0;
	//通讯处理线程ID，线程启动时获得
	m_dwCommDealThreadID = 0;

	// 分配服务器IP地址缓存
	m_chServerIP = new int8[64];

	//判断传入的IP地址 不为NUll 则保存服务器绑定IP地址
	if(	(chServerIP != NULL) && (strlen(chServerIP) < 60) )
	{
		//strcpy_s(m_chServerIP,64,chServerIP);
		strcpy(m_chServerIP,chServerIP);
	}

	//判断传入端口 不为0 则保存服务器绑定端口
	m_nServerPort = 554;
	if(	nServerPort != 0 )
	{
		m_nServerPort = nServerPort;
	}

	//服务器网络套接字
	m_nServerSocket = -1;

	//发送回应命令的缓冲区
	m_chSendCmd  = new int8[2048];

	//用于向服务程序发送消息的参数
	m_nMsgThreadID = nThreadID;
	m_nMsgID = nMsgID;

	//保存视频资源的链表
//	m_pVideoList  = new CPtrList();
//	m_pVideoList->RemoveAll();
	this->m_pVideoList = new list<void*>();
	this->m_pVideoList->clear();

	//保存客户端连接的链表
//	m_pClientList = new CPtrList();
//	m_pClientList->RemoveAll();
	this->m_pClientList = new list<void*>();
	this->m_pClientList->clear();

	//互斥对象用于处理视频资源连接访问时的同步
	//InitializeSRWLock(&m_pVideoSRWLock);
	pthread_rwlock_init(&m_pVideoSRWLock,NULL);

	//互斥对象用于处理客户端连接访问时的同步
	//InitializeSRWLock(&m_pClientSRWLock);
	pthread_rwlock_init(&m_pClientSRWLock,NULL);


	// 添加未初始化的参数
	this->m_chCurrentCSeq = NULL;
	this->m_nThreadFlag = 0;
	this->m_pCurClientInfo = NULL;

	// 初始化检测时间
	gettimeofday(&this->lastCheckTime,NULL);
}

/**
 * 析构函数
 */
CRtspServer::~CRtspServer()
{
	//调用Stop函数停止服务,如果有视频连接则断开所有连接;
	this->StopRun();

	//服务器IP缓冲区
	if(m_chServerIP !=NULL)
	{
		delete[] m_chServerIP;
		m_chServerIP = NULL;
	}
	//协议回应信息缓冲区
	if(m_chSendCmd !=NULL)
	{
		delete[] m_chSendCmd;
		m_chSendCmd = NULL;
	}
	//保存客户端连接的链表
	if(m_pClientList != NULL)
	{
		delete m_pClientList;
		m_pClientList = NULL;
	}
	//资源列表
	if(m_pVideoList != NULL)
	{
		//移除所有视频流
		RemoveStream("");
		//释放视频资源链表
		delete m_pVideoList;
		m_pVideoList = NULL;
	}
	return;
}

/*
函数功能：启动服务
函数接口：int8 StartRun();
出口参数：无
返 回 值：0：成功；1失败
函数功能：启动RTSP服务模块
*/
/**
 * 启动服务
 * @return 0：成功；1失败
 */
int8 CRtspServer::StartRun( )
{
	//判断TCP服务是否启动
	if(m_nServerSocket > 0)
		return 0;
	//调用TCP启动TCP服务器
	m_nServerSocket =	TCP_StartServer(this,m_chServerIP,(unsigned short*)&m_nServerPort,nMaxClient,DllConnectFuc);
	//打开本地监听端口失败
	if(m_nServerSocket <= 0)
		return 1;
	//线程启动 修改运行状态
	m_nThreadFlag = 1;
	//启动通讯处理线程

//	m_hCommDealThread = CreateThread(
//						NULL,					//安全属性
//						0,						//堆栈大小
//						&CommDealThread,		//线程函数
//						this,					//参数
//						0,						//马上运行
//						&m_dwCommDealThreadID	//线程ID
//						);
	m_dwCommDealThreadID = 3;
	pthread_create(
			&this->m_hCommDealThread,
			NULL,
			CommDealThread,
			this
	);

	//启动通讯处理线程失败
	if(m_hCommDealThread == NULL)
	{
		//关闭本地监听端口
		TCP_Close(m_nServerSocket);
		m_nServerSocket = 0;
		//线程启动失败 修改运行状态
		m_nThreadFlag = 0;
		return 1;//返回失败
	}
	return 0;
}

/*
函数功能：停止服务
函数接口：void StopRun();
函数功能：停止RTSP服务
*/
/**
 * 停止服务
 */
void CRtspServer::StopRun(void)
{
	//停止线程
	if(m_hCommDealThread != NULL)
	{
		do
		{
			//停止线程运行
			m_nThreadFlag = 0;
			//延时10毫秒
			//Sleep(10);
			usleep(10*1000);

		}while(m_hCommDealThread != 0);
		//线程ID赋值为0
		m_dwCommDealThreadID = 0;
	}
	//判断TCP服务是否活动
	if(m_nServerSocket > 0)
	{
		//调用TCP_Close函数关闭所TCP服务器
		TCP_Close(m_nServerSocket);
		m_nServerSocket = 0;
	}
	//客户端连接列表
	if(m_pClientList != NULL)
	{
//		//获取客户端链表头位置
//		POSITION nCurNode = m_pClientList->GetHeadPosition();
//		//循环处理所有客户端
//		for(;nCurNode != NULL;)
		for(list<void*>::iterator it = m_pClientList->begin(); it != m_pClientList->end() ;  it++)
		{
			//获取客户端实例
			//m_pCurClientInfo = (TCPClientInfo*)m_pClientList->GetNext(nCurNode);
			m_pCurClientInfo = (TCPClientInfo*)(*it);

			//客户端对象不为空
			if(m_pCurClientInfo != NULL)
			{
				//释放客户端对象
				RemoveRtspClient(m_pCurClientInfo);
			}
			continue;
		}

		//清空客户端链表
		//m_pClientList->RemoveAll();
		m_pClientList->clear();
	}
	//停止视频信息列表与前端设备的连接
	if (m_pVideoList != NULL)
	{
		StopVideo("");
	}
	return;
}

/**
 * 添加客户端节点
 * 	该函数由tcp服务器的回调函数调用
 *
 * @param nClientSock 客户端socket
 * @param chClientIP 客户端ip
 * @param nClientPort 客户端端口
 * @return
 */
int8 CRtspServer::AddRtspClient(int32 nClientSock, int8 * chClientIP, uint16 nClientPort)
{
	//通讯线程未启动，返回失败
	if(m_dwCommDealThreadID == 0)
	{
		printf( "Error   CRtspServer::AddRtspClient():   服务器主通讯线程未启动!\r\n" );
		return 0;
	}

	//声明客户端信息实例
	TCPClientInfo* pClientInfo = new TCPClientInfo();
	if(pClientInfo == NULL)
	{
		printf( "Error   CRtspServer::AddRtspClient():   客户端信息实例化失败，内存不足（TCPClientInfo分配失败）!\r\n" );
		return 0;
	}

	//初始化客户端信息
	pClientInfo->bActive = true;					//当前活动状态 true-正常 false-异常
	pClientInfo->nUserType = 4;						//客户端类型
	//strcpy_s(pClientInfo->chClientIP,20,chClientIP);//存储客户端IP信息
	strcpy(pClientInfo->chClientIP,chClientIP);//存储客户端IP信息
	pClientInfo->nClientIP = inet_addr(chClientIP);
	pClientInfo->nClientPort = nClientPort;			//存储客户端端口信息
	pClientInfo->nClientSock = nClientSock;			//存储连接套接字信息
	pClientInfo->chVideoName[0] = '\0';				//访问的流名称
	pClientInfo->nTransFlag	= 0;					//是否开始转发标识
	pClientInfo->mTranMode	= RTP_UDP;				//传输模式, 目前支持RTP_UDP、TRP_TCP模式
	memset(pClientInfo->chSessionID, 0x00, 64);		//Session信息
	pClientInfo->nRtpPort = 0;						//RTP数据通讯的端口号
	pClientInfo->nRtcpPort = 0;						//RTCP数据通讯的端口号
	pClientInfo->nRtpChannelId	= 0;				//RTP数据通道ID
	pClientInfo->nRtcpChannelId	= 0;				//RTCP数据通道ID
	pClientInfo->nRtpSocket		= 0;				//服务端发送rtp数据的套接字
	pClientInfo->nRtcpSocket	= 0;				//服务端发送和接收rtcp数据的套接字
	pClientInfo->nSendRtpPort	= 0;				//服务端发送rtp数据的端口号
	pClientInfo->nSendRtcpPort	= 0;				//服务端发送和接收rtp数据的端口号
	pClientInfo->pRequestBuffer = new uint8[4096+1024];	//接收数据缓存区
	if(pClientInfo->pRequestBuffer == NULL)
	{
		delete pClientInfo;
		pClientInfo = NULL;

		printf( "Error   CRtspServer::AddRtspClient():   客户端信息实例化失败，内存不足（pRequestBuffer分配失败）!\r\n" );
		return 0;
	}
	pClientInfo->nRequestBufSize = 4096;			//接收数据缓冲区大小
	pClientInfo->nBytesAlreay = 0;					//已接收数据的长度
	pClientInfo->nBytesLeft = 4096;					//剩余空间的大小

	// FIXME: 这是？？？？？？
	pClientInfo->pLastCRLF = &pClientInfo->pRequestBuffer[-3];

	gettimeofday(&pClientInfo->nLkTime, NULL);		//获取当前时间
	pClientInfo->pVideoInfo	= NULL;					//对应的资源信息

	// FIXME： 直接向队列中添加不可以吗？
//	//向通讯线程发送消息
//	if(0 != ::PostThreadMessage(m_dwCommDealThreadID, WM_CLIENT_ADD, 0, (LPARAM)pClientInfo))
//	{
//		return 1;
//	}
	//printf("添加到客户端列表中! 测试tcp服务端......\r\n");

	pthread_rwlock_wrlock(&m_pClientSRWLock);
	//m_pClientList->insert(m_pClientList->end(), pClientInfo);
	m_pClientList->push_back(pClientInfo);
	pthread_rwlock_unlock(&m_pClientSRWLock);
	return 1;
	// FIXME： 直接向队列中添加不可以吗？

	//消息发送失败 要释放分配的资源
	delete[] pClientInfo->pRequestBuffer;
	pClientInfo->pRequestBuffer = NULL;
	delete pClientInfo;pClientInfo = NULL;

	return 0;
}

//移除客户端节点
int8 CRtspServer::RemoveRtspClient(TCPClientInfo* pClientInfo)
{
	//是否已启动转发，如果已启动转发需删除转发信息
	if(pClientInfo->nTransFlag)
	{
		//判断客户端是否有对应视频资源对象
		if((pClientInfo->pVideoInfo != NULL) && (pClientInfo->pVideoInfo->pRtspClient != NULL))
		{
			//根据传输模式停止对本节点数据的转发
			if(pClientInfo->mTranMode == RTP_UDP)
			{
				//删除对节点的转发
				((CRtspClient*)(pClientInfo->pVideoInfo->pRtspClient))->RemoveSubNode(pClientInfo->nRtpSocket, pClientInfo->nClientIP, (uint16)(pClientInfo->nRtpPort));
				//关闭rtp和rtcp通讯端口
				ReleaseRTPandRTCP(pClientInfo->nRtpSocket, pClientInfo->nRtcpSocket);
			}
			else if(m_pCurClientInfo->mTranMode == RTP_TCP)
			{
				//删除对节点的转发
				((CRtspClient*)(pClientInfo->pVideoInfo->pRtspClient))->RemoveSubNode(pClientInfo->nRtpSocket, (uint8)(pClientInfo->nRtpChannelId));
			}
		}
		//初始化rtp和rtcp相关信息
		pClientInfo->nRtpSocket = -1;
		pClientInfo->nRtcpSocket= -1;
		pClientInfo->nSendRtpPort= 0;
		pClientInfo->nSendRtcpPort=0;
		//已停止转发 修改转发标志
		pClientInfo->nTransFlag = 0;
	}
	//释放缓存区
	delete[] pClientInfo->pRequestBuffer;
	pClientInfo->pRequestBuffer = NULL;
	//断开TCP连接
	if( pClientInfo->nClientSock > 0)
	{
		TCP_Close(pClientInfo->nClientSock);
		pClientInfo->nClientSock = 0;
	}
	//删除客户端信息
	delete[] pClientInfo; pClientInfo = NULL;

	return 0;
}

/*
函数功能：	移除客户端节点
函数接口：	int8 RemoveRtspClient(const int8* chVideoName, const int8* chNodeIP, const int32 nNodePort)
入口参数：	chVideoName	流的名称
			chNodeIP	节点IP
			nNodePort	节点端口
返 回 值：	0 成功; 1 失败;
*/
int8 CRtspServer::RemoveRtspClient(const int8* , const int8* chNodeIP, const uint16 nNodePort)
{
	//判断客户端链表指针是否为空
	if(m_pClientList == NULL) return 1;

	//互斥访问
	//AcquireSRWLockExclusive(&m_pClientSRWLock);
	pthread_rwlock_wrlock(&m_pClientSRWLock);

	//客户端连接信息
	TCPClientInfo* pClientInfo = NULL;
//	//获取链表头
//	POSITION pCurNodePos = m_pClientList->GetHeadPosition();
//	//循环获取链表各资源
//	for(;pCurNodePos != NULL;)
	for(list<void*>::iterator it = m_pClientList->begin(); it != m_pClientList->end() ;  it++)
	{
		//获取客户端连接信息
		//pClientInfo = (TCPClientInfo *)m_pClientList->GetNext(pCurNodePos);
		pClientInfo = (TCPClientInfo *)(*it);

		//匹配对应的客户端
		if((strcpy(pClientInfo->chClientIP, chNodeIP) == 0) && (pClientInfo->nClientPort == nNodePort))
		{
			//移除客户端对象
			RemoveRtspClient(pClientInfo);
		}
	}

	//释放自旋锁
	//ReleaseSRWLockExclusive(&m_pClientSRWLock);
	pthread_rwlock_unlock(&m_pClientSRWLock);

	//返回
	return 0;
}


//函数功能：视频信息拷贝
//函数接口：int8 videoInfoCpy(CVideoInfo * pDstInfo, CVideoInfo * pSrcInfo);
//入口参数：	pSrcInfo	源信息
//出口参数：	pDstInfo	目的信息
//返回值：	0：成功，1：失败；
int8 CRtspServer::videoInfoCpy(CVideoInfo * pDstInfo, CVideoInfo * pSrcInfo)
{
	//如果节点信息为空，返回失败
	if((pDstInfo == NULL) || (pSrcInfo == NULL))
		return 1;
	//资源信息赋值
	pDstInfo->nVideoID		= pSrcInfo->nVideoID;				//资源ID编号
	//strcpy_s(pDstInfo->chKey,32,pSrcInfo->chKey);				//资源的键值
	strcpy(pDstInfo->chKey,pSrcInfo->chKey);				//资源的键值
	//strcpy_s(pDstInfo->chVideoSrc,256,pSrcInfo->chVideoSrc);	//资源地址
	strcpy(pDstInfo->chVideoSrc,pSrcInfo->chVideoSrc);	//资源地址
	//strcpy_s(pDstInfo->chVideoSrv,256,pSrcInfo->chVideoSrv);	//资源服务地址
	strcpy(pDstInfo->chVideoSrv,pSrcInfo->chVideoSrv);	//资源服务地址
	pDstInfo->nVideoStatus	= pSrcInfo->nVideoStatus;			//资源状态
	//strcpy_s(pDstInfo->nYTType,32,pSrcInfo->nYTType);			//云台类型编号
	strcpy(pDstInfo->nYTType,pSrcInfo->nYTType);			//云台类型编号
	//strcpy_s(pDstInfo->chEncFormat,32,pSrcInfo->chEncFormat);	//视频编码格式
	strcpy(pDstInfo->chEncFormat,pSrcInfo->chEncFormat);	//视频编码格式
	//strcpy_s(pDstInfo->chSession,32,pSrcInfo->chSession);		//资源连接的Session信息
	strcpy(pDstInfo->chSession,pSrcInfo->chSession);		//资源连接的Session信息
	//strcpy_s(pDstInfo->chTrack,32,pSrcInfo->chTrack);			//播放控制的track信息
	strcpy(pDstInfo->chTrack,pSrcInfo->chTrack);			//播放控制的track信息
	//strcpy_s(pDstInfo->chUser,32,pSrcInfo->chUser);				//用户名
	strcpy(pDstInfo->chUser,pSrcInfo->chUser);				//用户名
	//strcpy_s(pDstInfo->chPwd,32,pSrcInfo->chPwd);				//密码
	strcpy(pDstInfo->chPwd,pSrcInfo->chPwd);				//密码
	pDstInfo->nVideoWidth	= pSrcInfo->nVideoWidth;			//视频画面宽度
	pDstInfo->nVideoHeight	= pSrcInfo->nVideoHeight;			//视频画面高度
	pDstInfo->nVideoFrames	= pSrcInfo->nVideoFrames;			//视频帧速
	pDstInfo->nVideoBps		= pSrcInfo->nVideoBps;				//视频比特率
	pDstInfo->pRtspClient	= pSrcInfo->pRtspClient;			//客户端代理指针
	pDstInfo->bOverTCP		= pSrcInfo->bOverTCP;				//是否使用TCP传输视频数据
	pDstInfo->nStartPort	= 0;								//记录节点起始端口

	//返回成功
	return 0;
}
/*
函数功能：添加、重设视频流
函数接口：int8 SetStream(int8* chVideoName, CVideoInfo* pNewInfo);
入口参数：	chVideoName	添加或设置流的名称
			pNewInfo	视频流信息
pInfo	资源信息
出口参数：无；
返 回 值：0：成功, 1：失败
函数功能：向RTSP服务端添加流,如果流名称已存在,则改变流信息
*/
int8 CRtspServer::SetStream(int8* chVideoName, CVideoInfo* pNewInfo)
{
	//判断链表指针是否为空
	if(m_pVideoList == NULL)
		return (1);
	//互斥访问
	//AcquireSRWLockExclusive(&m_pVideoSRWLock);
	pthread_rwlock_wrlock(&m_pVideoSRWLock);

	//是否找到视频节点
	bool bFound = false;
	//视频资源信息
	CVideoInfo* pVideoInfo = NULL;

//	//获取链表头
//	POSITION pCurNodePos = m_pVideoList->GetHeadPosition();
//	//循环获取链表各资源
//	for(;pCurNodePos != NULL;)
	for(list<void*>::iterator it = m_pVideoList->begin(); it != m_pVideoList->end();  it++)
	{
		//获取视频资源信息
		//pVideoInfo = (CVideoInfo *)m_pVideoList->GetNext(pCurNodePos);
		pVideoInfo = (CVideoInfo *)(*it);

		//判断资源是否含有该资源
		if(strcmp(pVideoInfo->chKey,chVideoName) == 0)
		{
			//找到视频资源 修改标志
			bFound = true;
			break;
		}
		//资源不匹配 赋值为空
		pVideoInfo = NULL;
	}
	//节点不存在
	if(pVideoInfo == NULL)
	{
		//创建一个资源节点
		pVideoInfo = new CVideoInfo;
		//资源节点创建失败
		if(pVideoInfo == NULL)
		{
			//释放自旋锁
			//ReleaseSRWLockExclusive(&m_pVideoSRWLock);
			pthread_rwlock_unlock(&m_pVideoSRWLock);

			//返回失败
			return 1;
		}
		//视频资源节点初始化
		memset(pVideoInfo,0x00,sizeof(CVideoInfo));
	}

	//拷贝资源信息
	videoInfoCpy(pVideoInfo,pNewInfo);

	//链表中不存在，则将信息添加到链表
	if(bFound == false)
	{
		//客户端代理指针
		pVideoInfo->pRtspClient = 0;
		//添加到资源列表
		//m_pVideoList->AddTail(pVideoInfo);
		//m_pVideoList->insert(m_pVideoList->end(),pVideoInfo);
		m_pVideoList->push_back(pVideoInfo);
	}
	//释放信号量
	//ReleaseSRWLockExclusive(&m_pVideoSRWLock);
	pthread_rwlock_unlock(&m_pVideoSRWLock);

	//返回成功
	return 0;
}
/*
函数功能：删除视频流
函数接口：int8 RemoveStream(int8* chVideoName);
入口参数：chVideoName	删除视频流的名称;
出口参数：无
返 回 值：0：成功, 1：失败；
函数功能：从RTSP服务端删除流
*/
int8 CRtspServer::RemoveStream(int8* chVideoName)
{
	//判断链表指针是否为空
	if(m_pVideoList == NULL)
	{
		return (1);
	}
	//互斥访问
	//AcquireSRWLockExclusive(&m_pVideoSRWLock);
	pthread_rwlock_wrlock(&m_pVideoSRWLock);

	//视频资源信息
	CVideoInfo* pVideoInfo = NULL;

//	//获取链表头
//	POSITION pPreNodePos;
//	POSITION pCurNodePos = m_pVideoList->GetHeadPosition();
//	//循环获取链表各资源
//	for(;pCurNodePos != NULL;)
	// 待删除元素
	list<void*>::iterator deleteit;
	for(list<void*>::iterator it = m_pVideoList->begin(); it != m_pVideoList->end() ;)
	{
		//保存当前节点
		//pPreNodePos = pCurNodePos;
		//获取视频资源信息
		//pVideoInfo = (CVideoInfo *)m_pVideoList->GetNext(pCurNodePos);
		pVideoInfo = (CVideoInfo *)(*it);

		//判断链表是否含有该资源
		if((chVideoName == NULL) || (chVideoName[0] == '\0') || (strcmp(pVideoInfo->chKey,chVideoName) == 0))
		{
			//从链表中删除当前视频节点
			//m_pVideoList->RemoveAt(pPreNodePos);
			///////////////////////////////////////////////////////////////////
			// 遍历中删除
			deleteit = it;
			it++;
			m_pVideoList->erase(deleteit); 	// 删除但前元素
			///////////////////////////////////////////////////////////////////


			//删除客户端与资源的对应关系
			ModifyClientByName(pVideoInfo);
			//释放视频资源的客户端
			if(pVideoInfo->pRtspClient!= NULL)
			{
				delete (CRtspClient*)(pVideoInfo->pRtspClient);
				pVideoInfo->pRtspClient = NULL;
			}
			//释放视频资源对象资源
			delete pVideoInfo;pVideoInfo = NULL;
		}
	}
	//释放互斥量
	//ReleaseSRWLockExclusive(&m_pVideoSRWLock);
	pthread_rwlock_unlock(&m_pVideoSRWLock);

	return (0);
}

/*
函数功能：删除视频资源对应的客户端连接对应关系
函数接口：void ModifyClientByName(const CVideoInfo* pVideoInfo);
入口参数：pVideoInfo	视频资源对象;
返 回 值：无
*/
void CRtspServer::ModifyClientByName(const CVideoInfo* pVideoInfo)
{
	//视频资源对象指针为空
	if(pVideoInfo == NULL)
		return;
	//判断客户端链表指针是否为空
	if(m_pClientList == NULL)
		return ;

	//互斥访问
	//AcquireSRWLockExclusive(&m_pClientSRWLock);
	pthread_rwlock_wrlock(&m_pClientSRWLock);

	//客户端连接信息
	TCPClientInfo* pClientInfo = NULL;

//	//获取链表头
//	POSITION pCurNodePos = m_pClientList->GetHeadPosition();
//	//循环获取链表各资源
//	for(;pCurNodePos != NULL;)
	for( list<void*>::iterator it = m_pClientList->begin(); it != m_pClientList->end() ;  it++)
	{
		//获取客户端连接信息
		//pClientInfo = (TCPClientInfo *)m_pClientList->GetNext(pCurNodePos);
		pClientInfo = (TCPClientInfo *)(*it);

		//判断客户端请求时视频资源，是否为当前资源
		if(pClientInfo->pVideoInfo == pVideoInfo)
		{
			//将客户端对应的资源指针赋值为空
			pClientInfo->pVideoInfo = NULL;
		}
	}
	//释放互斥量
	//ReleaseSRWLockExclusive(&m_pClientSRWLock);
	pthread_rwlock_unlock(&m_pClientSRWLock);

	return;
}
/*
函数功能：根据名字查找视频流
函数接口：void* FindVideoByName(int8 const* chVideoName);
入口参数：chVideoName	要查找的视频流的名称;
出口参数：无
返 回 值：0：失败, >0：视频资源对象；
函数功能：从RTSP服务端删除流
*/
void* CRtspServer::FindVideoByName(int8 const* chVideoName)
{
	//判断链表指针是否为空
	if(m_pVideoList == NULL)
	{
		return NULL;
	}
	//互斥访问
	//AcquireSRWLockExclusive(&m_pVideoSRWLock);
	pthread_rwlock_wrlock(&m_pVideoSRWLock);

	//视频资源信息
	CVideoInfo* pVideoInfo = NULL;
//	//获取链表头
//	POSITION pPreNodePos;
//	POSITION pCurNodePos = m_pVideoList->GetHeadPosition();
//	//循环获取链表各资源
//	for(;pCurNodePos != NULL;)
	for(list<void*>::iterator it = m_pVideoList->begin(); it != m_pVideoList->end();  it++)
	{
		//保存当前节点
		//pPreNodePos = pCurNodePos;
		//获取视频资源信息
		//pVideoInfo = (CVideoInfo *)m_pVideoList->GetNext(pCurNodePos);
		pVideoInfo = (CVideoInfo *)(*it);

		//判断链表是否含有该资源
		if((strcmp(pVideoInfo->chKey,chVideoName) == 0) && (pVideoInfo->nVideoStatus == 1))
		{
			break;
		}
		//资源不匹配 赋值为空
		pVideoInfo = NULL;
	}
	//释放信号量
	//ReleaseSRWLockExclusive(&m_pVideoSRWLock);
	pthread_rwlock_unlock(&m_pVideoSRWLock);

	//返回视频资源指针
	return (void*)pVideoInfo;
}
/*************************************************************
启动视频流
函数接口：	int8 StartVideo(int8* chVideoName);
入口参数：	chVideoName	视频流名称
出口参数：	无；
返 回 值：	0：成功,1：失败,2：部分启动成功
函数功能：	启动指定流服务
**************************************************************/
int8 CRtspServer::StartVideo(int8 * chVideoName)
{
	//判断链表指针是否为空
	if(m_pVideoList == NULL)
	{
		return (1);
	}
	//互斥访问
	//AcquireSRWLockExclusive(&m_pVideoSRWLock);
	pthread_rwlock_wrlock(&m_pVideoSRWLock);

	//视频资源信息
	CVideoInfo* pVideoInfo = NULL;

	//获取链表头
//	POSITION pCurNodePos = m_pVideoList->GetHeadPosition();
//	//循环查找对应结点
//	for(; pCurNodePos != NULL;)
	for(list<void*>::iterator it = m_pVideoList->begin(); it != m_pVideoList->end(); it++)
	{
		//若资源不为空，获取该资源
		//pVideoInfo = (CVideoInfo *)m_pVideoList->GetNext(pCurNodePos);
		pVideoInfo = (CVideoInfo *)(*it);

		//判断链表是否含有该资源
		if((chVideoName == NULL) || (chVideoName[0] == '\0') || (strcmp(pVideoInfo->chKey,chVideoName) == 0))
		{
			//解码器IP地址
			int8  chSrcIP[20];
			//解码器端口
			int32 nSrcPort;
			//获取资源的IP和端口
			if(true == anlayConStr(pVideoInfo->chVideoSrc,chSrcIP,&nSrcPort))
			{
				//客户端未创建 创建客户端
				if(pVideoInfo->pRtspClient == NULL)
				{
					//创建RTSP客户端
					if(pVideoInfo->bOverTCP)
					{
						pVideoInfo->pRtspClient = new CRtspClient("rtspclient", 2, NULL, pVideoInfo->chUser, pVideoInfo->chPwd, true);
					}
					else
					{
						pVideoInfo->pRtspClient = new CRtspClient("rtspclient", 2, NULL, pVideoInfo->chUser, pVideoInfo->chPwd, false);
					}
				}
				//RTSP客户端创建成功 启动客户端
				if(pVideoInfo->pRtspClient != NULL)
				{
					//初始化
					((CRtspClient*)(pVideoInfo->pRtspClient))->Init((char*)chSrcIP, (uint16)nSrcPort, pVideoInfo->chKey, pVideoInfo->chVideoSrc, NULL);
					//上报参数
					((CRtspClient*)(pVideoInfo->pRtspClient))->SetReportParam(pVideoInfo->nVideoID, m_nMsgThreadID, m_nMsgID);
					//启动RTSP客户端代理
					((CRtspClient*)(pVideoInfo->pRtspClient))->StartRun();
					//资源状态
					pVideoInfo->nVideoStatus = 1;
				}
			}
			//如果不是启动全部资源 则退出循环
			if((chVideoName == NULL) || (chVideoName[0] == '\0'))
			{
				break;
			}
		}
	}
	//释放信号量
	//ReleaseSRWLockExclusive(&m_pVideoSRWLock);
	pthread_rwlock_unlock(&m_pVideoSRWLock);

	//返回成功
	return (0);
}
/*************************************************************
停止流服务
函数接口：	int8 StopVideo(int8* chVideoName);
入口参数：	chVideoName	添加或设置流的名称;
出口参数：	无；
返 回 值：	0：成功，1：失败；
函数功能：	停止指定流服务，名称未指定时停止所有流服务(修改状态为0，停止)。
*************************************************************/
int8 CRtspServer::StopVideo(int8* chVideoName)
{
	//判断链表指针是否为空
	if(m_pVideoList == NULL)
	{
		return (1);
	}
	//互斥访问
	//AcquireSRWLockExclusive(&m_pVideoSRWLock);
	pthread_rwlock_wrlock(&m_pVideoSRWLock);

	//视频资源信息
	CVideoInfo* pVideoInfo = NULL;

//	//获取链表头
//	POSITION pCurNodePos = m_pVideoList->GetHeadPosition();
//	//循环查找对应结点
//	for(; pCurNodePos != NULL;)
	for( list<void*>::iterator it = m_pVideoList->begin(); it != m_pVideoList->end(); it++)
	{
		//若资源不为空，获取该资源
		//pVideoInfo = (CVideoInfo *)m_pVideoList->GetNext(pCurNodePos);
		pVideoInfo = (CVideoInfo *)(*it);

		//判断链表是否含有该资源
		if((chVideoName == NULL) || (chVideoName[0] == '\0') || (strcmp(pVideoInfo->chKey,chVideoName) == 0))
		{
			//向修改视频资源状态
			pVideoInfo->nVideoStatus = 0;
			//删除客户端与资源的对应关系
			ModifyClientByName(pVideoInfo);
			//如果客户端对象已创建 释放客户端
			if(pVideoInfo->pRtspClient != NULL)
			{
				//删除客户端代理
				delete (CRtspClient*)(pVideoInfo->pRtspClient);
				pVideoInfo->pRtspClient = NULL;
			}
			//如果不是启动全部资源 则退出循环
			if((chVideoName == NULL) || (chVideoName[0] == '\0'))
			{
				break;
			}
		}
	}
	//释放信号量
	//ReleaseSRWLockExclusive(&m_pVideoSRWLock);
	pthread_rwlock_unlock(&m_pVideoSRWLock);

	//返回成功
	return (0);
}

/*
函数功能：通讯处理线程
函数接口：static DWORD CommDealThread (LPVOID pParam);
入口参数：pParam	线程启动参数,RTSP服务端实例指针。
出口参数：无
返 回 值：0：线程正常退出,1：线程异常退出。
函数功能：通讯处理线程，接收RTSP协议数据，处理视频数据转发
*/
/**
 * RTSPServer的TCP的通讯处理线程
 * @param pParam CRtspServer*自身指针。
 */
void*  CRtspServer::CommDealThread (void* pParam)
{
	//RTSP服务器端
	CRtspServer* pServer = (CRtspServer*)pParam;
	//RTSP客户端信息
	TCPClientInfo* pClientInfo = NULL;

	//用于接收消息
	//MSG msg;
	//是否退出
	while(pServer->m_nThreadFlag)
	{
		// 移动到 AddRtspClient使用锁完成。
//		// 获取线程消息
//		if(FALSE != ::PeekMessage(&msg,NULL,0,0xFFFFFFFF,PM_REMOVE))
//		{
//			if(msg.message != WM_CLIENT_ADD)
//				continue;
//			//将TCP客户端加入到客户端链表
//			pClientInfo = (TCPClientInfo*)(msg.lParam);
//			//pServer->m_pClientList->AddTail(pClientInfo);
//			pServer->m_pClientList.insert(pServer->m_pClientList.end(), pClientInfo);
//
//			continue;
//		}

		//处理通讯数据
		pServer->DealTCPData();

		// ClientCheck 定时检查客户端在线信息
		pServer->ClientCheck();

		//添加1毫秒延时 防止CPU使用太高
		//Sleep(1);
		usleep(1*1000);
	}
	//关闭线程句柄
	//CloseHandle(pServer->m_hCommDealThread);
	//FIXME: pthread close.

	pServer->m_hCommDealThread = NULL;
	return 0;
}

void CRtspServer::ClientCheck()
{
	// 获取当前时间
	struct timeval curtime;
	gettimeofday( &curtime, NULL );

	long sec = curtime.tv_sec - this->lastCheckTime.tv_sec;
	long delt  = sec * 1000000 + curtime.tv_usec - this->lastCheckTime.tv_usec;

	if( delt >= 10*1000000 )
	{
		gettimeofday( &this->lastCheckTime, NULL );

		// 定时输出检查信息
		printf( "Info >> 当前在线RTSP客户端个数：[ %d ]\r\n", m_pClientList->size() );
	}
}

// FIXME : 验证代码正确性
/**
 * 处理TCP通讯数据
 * @return
 */
int8 CRtspServer::DealTCPData()
{
	//列表内没有元素，直接返回
	if((m_pClientList == NULL))
	{
		return (-1);
	}

	//互斥访问
	//AcquireSRWLockExclusive(&m_pClientSRWLock);
	pthread_rwlock_wrlock(&m_pClientSRWLock);

	//printf("0---------客户端列表size: %d \r\n",m_pClientList->size());

	//循环处理所有客户端
	//for(;nCurNode != NULL;)
	// 直接迭代
	int i = 0;

	// 待删除元素的迭代器
	list<void*>::iterator deleteit;

	// 遍历所有的客户端，循环处理每一个客户端的RTSP协议数据。
	// 遍历元素，特别注意，针对遍历过程中的删除操作。
	//for(list<void*>::iterator it = m_pClientList->begin(); it != m_pClientList->end(); it++)
	for(list<void*>::iterator it = m_pClientList->begin(); it != m_pClientList->end(); )
	{
		//i++;

		//printf("i = %d 客户端列表size: %d \r\n",i,m_pClientList->size());

		//获取客户端实例
		//m_pCurClientInfo = (TCPClientInfo*)m_pClientList->GetNext(nCurNode);
		m_pCurClientInfo = NULL;
		m_pCurClientInfo = (TCPClientInfo*)(*it);

		//printf("处理第%d个客户端数据%0x \r\n", i , m_pCurClientInfo);
		//printf("处理第%d个客户端数据: ip-%s,port-%d \r\n",i, m_pCurClientInfo->chClientIP, m_pCurClientInfo->nClientPort);

		if(m_pCurClientInfo == NULL)
		{
			//移除客户端
			printf("删除空的客户端[%d]\r\n",i);

			//m_pClientList->RemoveAt(nPreNode);

			///////////////////////////////////////////////////////////////////
			// 遍历中删除
			deleteit = it;
			it++;
			m_pClientList->erase(deleteit); 	// 删除但前元素
			///////////////////////////////////////////////////////////////////

			printf("删除空的客户端[%d]成功! \r\n",i);

			// 继续遍历
			continue;
		}

		//接收客户端的请求数据
		int nBytesRead = TCP_Recv(m_pCurClientInfo->nClientSock, (char*)&m_pCurClientInfo->pRequestBuffer[m_pCurClientInfo->nBytesAlreay], m_pCurClientInfo->nBytesLeft, 0);

		//printf(" *********** > nBytesRead = %d \r\n",nBytesRead);

		//RTP_UDP模式 或 未开始传输视频数据时 检查连接
		if((m_pCurClientInfo->nTransFlag == 0) || (m_pCurClientInfo->mTranMode == RTP_UDP))
		{
			//有效数据长度
			int32 nNewByteRead =0;
			//去掉空字符
			for(int i=0;i<nBytesRead;i++)
			{
				if(m_pCurClientInfo->pRequestBuffer[m_pCurClientInfo->nBytesAlreay + i] == '\0')
				{
					continue;
				}
				m_pCurClientInfo->pRequestBuffer[m_pCurClientInfo->nBytesAlreay + nNewByteRead] = m_pCurClientInfo->pRequestBuffer[m_pCurClientInfo->nBytesAlreay + i];
				nNewByteRead++;
			}
			//添加结束符
			m_pCurClientInfo->pRequestBuffer[m_pCurClientInfo->nBytesAlreay + nNewByteRead] = '\0';
			//处理RTSP请求数据
			handleRequestBytes(nNewByteRead);
		}
		//连接断开设置活动状态
		if(nBytesRead < 0)
		{
			m_pCurClientInfo->bActive = false;
		}
		//判断节点是否断开
		if((m_pCurClientInfo->nClientSock > 0) && (m_pCurClientInfo->bActive == true))
		{
			// 继续下一个客户端的信息处理
			it++;
			continue;
		}
		else
		{
			printf(" ******* 客户端已经断开！\r\n");
		}

		//客户端连接断开 移除客户端
		//m_pClientList->RemoveAt(nPreNode);

		///////////////////////////////////////////////////////////////////
		// 遍历中删除
		printf("删除已经断开的客户端[%d]， ip-%s,port-%d \r\n",i, m_pCurClientInfo->chClientIP, m_pCurClientInfo->nClientPort);
		deleteit = it;
		it++;
		m_pClientList->erase(deleteit);


		///////////////////////////////////////////////////////////////////

		//释放客户端对象
		RemoveRtspClient(m_pCurClientInfo);

		continue;
	}
	//释放互斥量
	//ReleaseSRWLockExclusive(&m_pClientSRWLock);
	pthread_rwlock_unlock(&m_pClientSRWLock);

	return 1;
}

/************************************************
函数功能：从传入的的连接字符串 解析出IP 和 port 返回
输入参数：字符格式为 XXXX://IP:port/XXXX
返回参数：IP存储解析的IP地址 port存储解析的端口信息
返回值：  成功返回true 失败返回false
*************************************************/
bool anlayConStr(int8* chStreamURL,int8* IP,int32* port)
{
	int8* startIndex = 0 ; //开始索引
	int8* endIndex = 0;		//结束索引
	int8 chPort[6];
	//设置默认端口
	*port = 554;
	memset(IP,0x00,20);
	memset(chPort,0x00,6);
	//查找字符中含有‘://’的位置记录到startIndex中
	int8* p = strchr(chStreamURL,':');
	if (p ==NULL) return false;
	int8* q = strchr(p+1,'/');//然后查找/
	if (p ==NULL) return false;
	q = strchr(q+1,'/');//然后查找/
	if (p ==NULL) return false;
	startIndex = q+1;

	//记录‘://’之后第一次出现‘:’位置
	endIndex = strchr(q,':');
	//如果输入参数格式为XXXX://IP/XXXX 则返回IP和默认端口554
	if (endIndex == NULL)
	{
		endIndex = strchr((q+1),'/');
		if(endIndex == NULL)
			return false;
		//‘://’和‘:’之间存放的就是IP
		int len = (int)(strlen(startIndex) - strlen(endIndex));
		memcpy(IP,startIndex,len);
		//*port = 554;
		return true;
	}

	//‘://’和‘:’之间存放的就是IP
	int len = (int)(strlen(startIndex) - strlen(endIndex));
	memcpy(IP,startIndex,len);

	//IP之后的‘:’和‘/’之间存放的就是端口号
	startIndex = endIndex+1;
	endIndex = strchr(startIndex,'/');
	if (endIndex == NULL) return false;
	len = (int)(strlen(startIndex) - strlen(endIndex));
	memcpy(chPort,startIndex,len);

	//将端口号转换成int
	*port = atoi(chPort);

	return true;
}
//组织RTSP回应信息
void CRtspServer::setRTSPResponse(char const* responseStr)
{
	char strDateHeader[200];
	//组织时间头信息
	time_t tt = time(NULL);
	strftime(strDateHeader, sizeof strDateHeader, "Date: %a, %b %d %Y %H:%M:%S GMT\r\n", gmtime(&tt));
	//组织回应数据
	//sprintf_s(m_chSendCmd, 2048,
	sprintf(m_chSendCmd,
		"RTSP/1.0 %s\r\n"
		"CSeq: %s\r\n"
		"%s\r\n",
		responseStr,
		m_chCurrentCSeq,
		strDateHeader);
}
void CRtspServer::setRTSPResponse(char const* responseStr, unsigned long sessionId)
{
	char strDateHeader[200];
	//组织时间头信息
	time_t tt = time(NULL);
	strftime(strDateHeader, sizeof strDateHeader, "Date: %a, %b %d %Y %H:%M:%S GMT\r\n", gmtime(&tt));
	//组织回应信息
	//sprintf_s(m_chSendCmd, 2048,
	sprintf(m_chSendCmd,
		"RTSP/1.0 %s\r\n"
		"CSeq: %s\r\n"
		"%s"
		"Session: %08X\r\n\r\n",
	   responseStr,
	   m_chCurrentCSeq,
	   strDateHeader,
	   sessionId);
}
void CRtspServer::setRTSPResponse(char const* responseStr, char const* contentStr)
{
	char strDateHeader[200];
	//组织时间头信息
	time_t tt = time(NULL);
	strftime(strDateHeader, sizeof strDateHeader, "Date: %a, %b %d %Y %H:%M:%S GMT\r\n", gmtime(&tt));
	if (contentStr == NULL) contentStr = "";
	unsigned int const contentLen = (int)strlen(contentStr);
	//组织回应信息
	//sprintf_s(m_chSendCmd, 2048,
	sprintf(m_chSendCmd,
		"RTSP/1.0 %s\r\n"
		"CSeq: %s\r\n"
		"%s"
		"Content-Length: %d\r\n\r\n"
		"%s",
		responseStr,
		m_chCurrentCSeq,
		strDateHeader,
		contentLen,
		contentStr);
}
void CRtspServer::setRTSPResponse(char const* responseStr, unsigned long sessionId, char const* contentStr)
{
	char strDateHeader[200];
	//组织时间头信息
	time_t tt = time(NULL);
	strftime(strDateHeader, sizeof strDateHeader, "Date: %a, %b %d %Y %H:%M:%S GMT\r\n", gmtime(&tt));
	if (contentStr == NULL) contentStr = "";
	unsigned int const contentLen = (int)strlen(contentStr);
	//组织回应信息
	//sprintf_s(m_chSendCmd, 2048,
	sprintf(m_chSendCmd,
		"RTSP/1.0 %s\r\n"
		"CSeq: %s\r\n"
		"%s"
		"Session: %08X\r\n"
		"Content-Length: %d\r\n\r\n"
		"%s",
		responseStr,
		m_chCurrentCSeq,
		strDateHeader,
		sessionId,
		contentLen,
		contentStr);
}
//RTSP命令数据错误的回应
void CRtspServer::handleCmd_bad()
{
	char strDateHeader[200];
	//组织时间头信息
	time_t tt = time(NULL);
	strftime(strDateHeader, sizeof strDateHeader, "Date: %a, %b %d %Y %H:%M:%S GMT\r\n", gmtime(&tt));
	//组织回应信息
	//sprintf_s(m_chSendCmd, 2048,
	sprintf(m_chSendCmd,
		"RTSP/1.0 400 Bad Request\r\n%sAllow: %s\r\n\r\n",
		strDateHeader, g_AllowedCommandNames);
	//设置当前客户端活动状态
	m_pCurClientInfo->bActive = false;
}
//RTSP命令不支持的回应
void CRtspServer::handleCmd_notSupported()
{
	char strDateHeader[200];
	//组织时间头信息
	time_t tt = time(NULL);
	strftime(strDateHeader, sizeof strDateHeader, "Date: %a, %b %d %Y %H:%M:%S GMT\r\n", gmtime(&tt));
	//组织回应信息
	//sprintf_s(m_chSendCmd, 2048,
	sprintf(m_chSendCmd,
		"RTSP/1.0 405 Method Not Allowed\r\nCSeq: %s\r\n%sAllow: %s\r\n\r\n",
		m_chCurrentCSeq, strDateHeader, g_AllowedCommandNames);
	//2016-1-23,yxj，不需要断开服务
	//设置当前客户端活动状态
	//m_pCurClientInfo->bActive = false;
}
//流未找到的回应
void CRtspServer::handleCmd_notFound()
{
	setRTSPResponse("404 Stream Not Found");
	//设置当前客户端活动状态
	m_pCurClientInfo->bActive = false;
}
//Session未找到的回应
void CRtspServer::handleCmd_sessionNotFound()
{
	setRTSPResponse("454 Session Not Found");
	//设置当前客户端活动状态
	m_pCurClientInfo->bActive = false;
}
//不支持的传输参数的回应
void CRtspServer::handleCmd_unsupportedTransport()
{
	setRTSPResponse("461 Unsupported Transport");
	//设置当前客户端活动状态
	m_pCurClientInfo->bActive = false;
}

//组织请求可用操作命令的回应
void CRtspServer::handleCmd_OPTIONS()
{
	char strDateHeader[200];
	//组织时间头信息
	time_t tt = time(NULL);
	strftime(strDateHeader, sizeof strDateHeader, "Date: %a, %b %d %Y %H:%M:%S GMT\r\n", gmtime(&tt));
	//组织可用操作命令的回应数据
	//sprintf_s(m_chSendCmd, 2048, "RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sPublic: %s\r\n\r\n", m_chCurrentCSeq, strDateHeader, g_AllowedCommandNames);
	sprintf(m_chSendCmd, "RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sPublic: %s\r\n\r\n", m_chCurrentCSeq, strDateHeader, g_AllowedCommandNames);
}
//组织请求资源描述命令的回应
void CRtspServer::handleCmd_DESCRIBE(char const* urlPreSuffix, char const* urlSuffix, char const* )
{
	do
	{
		//组织视频名称
		char urlTotalSuffix[RTSP_PARAM_STRING_MAX];
		if (strlen(urlPreSuffix) + strlen(urlSuffix) + 2 > sizeof urlTotalSuffix)
		{
			handleCmd_bad();
			break;
		}
		urlTotalSuffix[0] = '\0';
		if (urlPreSuffix[0] != '\0')
		{
			strcat(urlTotalSuffix, urlPreSuffix);
			strcat(urlTotalSuffix, "/");
		}
		strcat(urlTotalSuffix, urlSuffix);
		//查找资源信息
		if(m_pCurClientInfo->pVideoInfo != NULL)
		{
			//比较视频名称是否匹配
			if(strcmp(m_pCurClientInfo->pVideoInfo->chKey,urlTotalSuffix) != 0)
			{
				//通知流节点停止转发
				if(m_pCurClientInfo->nTransFlag)
				{
					//根据传输模式停止对本节点数据的转发
					if(m_pCurClientInfo->mTranMode == RTP_UDP)
					{
						//删除对节点的转发
						((CRtspClient*)(m_pCurClientInfo->pVideoInfo->pRtspClient))->RemoveSubNode(m_pCurClientInfo->nRtpSocket, m_pCurClientInfo->nClientIP, (uint16)(m_pCurClientInfo->nRtpPort));
						//关闭rtp和rtcp通讯端口
						ReleaseRTPandRTCP(m_pCurClientInfo->nRtpSocket, m_pCurClientInfo->nRtcpSocket);
					}
					else if(m_pCurClientInfo->mTranMode == RTP_TCP)
					{
						//删除对节点的转发
						((CRtspClient*)(m_pCurClientInfo->pVideoInfo->pRtspClient))->RemoveSubNode(m_pCurClientInfo->nRtpSocket, (uint8)(m_pCurClientInfo->nRtpChannelId));
					}
					//初始化rtp和rtcp相关信息
					m_pCurClientInfo->nRtpSocket = -1;
					m_pCurClientInfo->nRtcpSocket= -1;
					m_pCurClientInfo->nSendRtpPort= 0;
					m_pCurClientInfo->nSendRtcpPort=0;
				}
				//启动转发标志
				m_pCurClientInfo->nTransFlag = 0;
				//视频资源对象指针赋值为NULL
				m_pCurClientInfo->pVideoInfo = NULL;
			}
		}
		//如果客户端对应视频资源对象为空 根据名字查找视频资源对象
		if(m_pCurClientInfo->pVideoInfo == NULL)
		{
			//查找对应名称的视频资源
			m_pCurClientInfo->pVideoInfo = (CVideoInfo *)FindVideoByName(urlTotalSuffix);
			//视频资源未找到 或视频资源未正常连接
			if (m_pCurClientInfo->pVideoInfo == NULL)
			{
				handleCmd_notFound();
				break;
			}
		}
		//保存请求的流名称 为了后续验证
		//strcpy_s(m_pCurClientInfo->chVideoName, 200, urlTotalSuffix);
		strcpy(m_pCurClientInfo->chVideoName, urlTotalSuffix);
		//视频资源连接未创建或视频传输异常
		if((m_pCurClientInfo->pVideoInfo->pRtspClient == NULL) || (((CRtspClient*)(m_pCurClientInfo->pVideoInfo->pRtspClient))->m_nVideoFlag != 5))
		{
			//发送错误回应
			handleCmd_bad();
			break;
		}
		//保存sdp信息的缓冲区
		char sdpDescription[1024];
		//SDP描述信息格式
		char const* const sdpPrefixFmt =
			"v=0\r\n"
			"o=- %ld%06ld %d IN IP4 %s\r\n"
			"s=%s\r\n"
			"i=%s\r\n"
			"t=0 0\r\n"
			"a=tool:%s%s\r\n"
			"a=type:broadcast\r\n"
			"a=control:*\r\n"
			"a=range:npt=0-\r\n"
			"a=x-qt-text-nam:%s\r\n"
			"a=x-qt-text-inf:%s\r\n"
			"m=video 0 RTP/AVP %d\r\n"
			"b=AS:500\r\n"
			"a=rtpmap:%d %s/90000\r\n"
			"a=control:track1\r\n"
			"\r\n";

		//生成SDP描述信息
		//sprintf_s(sdpDescription, 1024, sdpPrefixFmt,
		sprintf(sdpDescription, sdpPrefixFmt,
			m_pCurClientInfo->nLkTime.tv_sec, m_pCurClientInfo->nLkTime.tv_usec,
			1,							// o= <version> // (needs to change if params are modified)
			m_chServerIP,				// o= <address>
			"chStreamName",				// s= <description>
			"chStreamName",				// i= <info>
			libNameStr, libVersionStr,	// a=tool:
			"P2PVideoServer",			// a=x-qt-text-nam: line
			"chStreamName",				// a=x-qt-text-inf: line
			((CRtspClient*)(m_pCurClientInfo->pVideoInfo->pRtspClient))->m_nRTPPayloadFormat,
			((CRtspClient*)(m_pCurClientInfo->pVideoInfo->pRtspClient))->m_nRTPPayloadFormat,
			m_pCurClientInfo->pVideoInfo->chEncFormat);

		//获取描述信息长度
		unsigned int sdpDescriptionSize = (int)strlen(sdpDescription);

		char strDateHeader[200];
		//组织时间头信息
		time_t tt = time(NULL);
		strftime(strDateHeader, sizeof strDateHeader, "Date: %a, %b %d %Y %H:%M:%S GMT\r\n", gmtime(&tt));
		//组织回应信息
		//sprintf_s(m_chSendCmd, 2048,
		sprintf(m_chSendCmd,
			"RTSP/1.0 200 OK\r\nCSeq: %s\r\n"
			"%s"
			"Content-Base: %s/\r\n"
			"Content-Type: application/sdp\r\n"
			"Content-Length: %d\r\n\r\n"
			"%s",
			m_chCurrentCSeq,
			strDateHeader,
			m_pCurClientInfo->pVideoInfo->chVideoSrv,
			sdpDescriptionSize,
			sdpDescription);
	} while (0);
}


static void parseTransportHeader(char const* buf,
								 StreamingMode& streamingMode,
								 char*& streamingModeString,
								 char*& destinationAddressStr,
								 unsigned char& destinationTTL,
								 unsigned short& clientRTPPortNum,	// if UDP
								 unsigned short& clientRTCPPortNum,	// if UDP
								 unsigned char& rtpChannelId,		// if TCP
								 unsigned char& rtcpChannelId		// if TCP
								 )
{
	// Initialize the result parameters to default values:
	streamingMode = RTP_UDP;
	streamingModeString = NULL;
	destinationAddressStr = NULL;
	destinationTTL = 255;
	clientRTPPortNum = 0;
	clientRTCPPortNum = 0;
	rtpChannelId = rtcpChannelId = 0xFF;

	unsigned short p1, p2;
	unsigned int ttl, rtpCid, rtcpCid;

	//查找Transport信息
	while (1)
	{
		if (*buf == '\0') return;
		if (*buf == '\r' && *(buf+1) == '\n' && *(buf+2) == '\r') return;
		//if (_strnicmp(buf, "Transport:", 10) == 0) break;
		if (strncasecmp(buf, "Transport:", 10) == 0) break;
		++buf;
	}

	//解析通讯参数
	char const* fields = buf + 10;
	while (*fields == ' ') ++fields;
	char* field = strDupSize(fields);
	while (sscanf(fields, "%[^;\r\n]", field) == 1)
	{
		if (strcmp(field, "RTP/AVP/TCP") == 0)
		{
			streamingMode = RTP_TCP;
		//} else if (_strnicmp(field, "destination=", 12) == 0)
		} else if (strncasecmp(field, "destination=", 12) == 0)
		{
			delete[] destinationAddressStr;
			destinationAddressStr = strDup(field+12);
		}
		else if (sscanf(field, "ttl%u", &ttl) == 1)
		{
			destinationTTL = (unsigned char)ttl;
		}
		else if (sscanf(field, "client_port=%hu-%hu", &p1, &p2) == 2)
		{
			clientRTPPortNum = p1;
			clientRTCPPortNum = streamingMode == RAW_UDP ? 0 : p2; // ignore the second port number if the client asked for raw UDP
		}
		else if (sscanf(field, "client_port=%hu", &p1) == 1)
		{
			clientRTPPortNum = p1;
			clientRTCPPortNum = streamingMode == RAW_UDP ? 0 : p1 + 1;
		}
		else if (sscanf(field, "interleaved=%u-%u", &rtpCid, &rtcpCid) == 2) {
			rtpChannelId = (unsigned char)rtpCid;
			rtcpChannelId = (unsigned char)rtcpCid;
		}

		fields += strlen(field);
		while (*fields == ';' || *fields == ' ' || *fields == '\t')
		{
			++fields; // skip over separating ';' chars or whitespace
		}
		if (*fields == '\0' || *fields == '\r' || *fields == '\n')
		{
			break;
		}
	}
	delete[] field;
}
//组织建立连接命令的回应
void CRtspServer::handleCmd_SETUP(char const* urlPreSuffix, char const* , char const* fullRequestStr)
{
	//流名称
	char const* streamName = urlPreSuffix;
	//查找视频流是否存在
	if(m_pCurClientInfo->pVideoInfo != NULL)
	{
		//比较视频名称是否匹配
		if(strcmp(m_pCurClientInfo->pVideoInfo->chKey,streamName) != 0)
		{
			//通知流节点停止转发
			if(m_pCurClientInfo->nTransFlag)
			{
				//根据传输模式停止对本节点数据的转发
				if(m_pCurClientInfo->mTranMode == RTP_UDP)
				{
					//删除对节点的转发
					((CRtspClient*)(m_pCurClientInfo->pVideoInfo->pRtspClient))->RemoveSubNode(m_pCurClientInfo->nRtpSocket, m_pCurClientInfo->nClientIP, (uint16)(m_pCurClientInfo->nRtpPort));
					//关闭rtp和rtcp通讯端口
					ReleaseRTPandRTCP(m_pCurClientInfo->nRtpSocket, m_pCurClientInfo->nRtcpSocket);
				}
				else if(m_pCurClientInfo->mTranMode == RTP_TCP)
				{
					//删除对节点的转发
					((CRtspClient*)(m_pCurClientInfo->pVideoInfo->pRtspClient))->RemoveSubNode(m_pCurClientInfo->nRtpSocket, (uint8)(m_pCurClientInfo->nRtpChannelId));
				}
				//初始化rtp和rtcp相关信息
				m_pCurClientInfo->nRtpSocket = -1;
				m_pCurClientInfo->nRtcpSocket= -1;
				m_pCurClientInfo->nSendRtpPort= 0;
				m_pCurClientInfo->nSendRtcpPort=0;
				//启动转发标志
				m_pCurClientInfo->nTransFlag = 0;
			}
			//视频资源对象指针赋值为NULL
			m_pCurClientInfo->pVideoInfo = NULL;
		}
	}
	//如果客户端对应视频资源对象为空 根据名字查找视频资源对象
	if(m_pCurClientInfo->pVideoInfo == NULL)
	{
		//查找对应名称的视频资源
		m_pCurClientInfo->pVideoInfo = (CVideoInfo *)FindVideoByName(streamName);
		//视频资源未找到 或视频资源未正常连接
		if (m_pCurClientInfo->pVideoInfo == NULL)
		{
			handleCmd_notFound();
			return;
		}
	}

	//解析传输参数
	StreamingMode streamingMode;
	char* streamingModeString = NULL; // set when RAW_UDP streaming is specified
	char* clientsDestinationAddressStr;
	unsigned char clientsDestinationTTL;
	unsigned short clientRTPPortNum, clientRTCPPortNum;
	unsigned char  rtpChannelId, rtcpChannelId;
	parseTransportHeader(fullRequestStr, streamingMode, streamingModeString,
		clientsDestinationAddressStr, clientsDestinationTTL,
		clientRTPPortNum, clientRTCPPortNum,
		rtpChannelId, rtcpChannelId);
	if ((streamingMode == RTP_TCP) && (rtpChannelId == 0xFF))
	{
		streamingMode = RTP_TCP;
		rtpChannelId  = 2; rtcpChannelId = 3;
	}
	//更新客户端传输端口
	if (streamingMode == RTP_UDP)
	{
		m_pCurClientInfo->nRtpPort = clientRTPPortNum;
		m_pCurClientInfo->nRtcpPort= clientRTCPPortNum;
		m_pCurClientInfo->nRtpChannelId = 0;
		m_pCurClientInfo->nRtcpChannelId = 0;
		//如果端口解析错误,传输模式设置未不支持的模式,不进行数据传输
		if(clientRTPPortNum == 0)
		{
			streamingMode = NONSUPPORT;
		}
	}
	else if (streamingMode == RTP_TCP)
	{
		m_pCurClientInfo->nRtpPort = 0;
		m_pCurClientInfo->nRtcpPort= 0;
		m_pCurClientInfo->nRtpChannelId = rtpChannelId;
		m_pCurClientInfo->nRtcpChannelId = rtcpChannelId;
	}
	//保存传输模式
	m_pCurClientInfo->mTranMode = streamingMode;
	//时间信息头
	char strDateHeader[200];
	//组织时间头信息
	time_t tt = time(NULL);
	strftime(strDateHeader, sizeof strDateHeader, "Date: %a, %b %d %Y %H:%M:%S GMT\r\n", gmtime(&tt));
	//组织回应信息
	switch (streamingMode)
	{
		//UDP通讯方式
	case RTP_UDP:
		{
			//初始化服务器端rtp和rtcp数据端口
			if((m_pCurClientInfo->nRtpSocket <= 0) && (false == InitiateRTPandRTCP(false, m_pCurClientInfo->nRtpSocket, m_pCurClientInfo->nRtcpSocket, m_chServerIP, m_pCurClientInfo->nSendRtpPort, m_pCurClientInfo->nSendRtcpPort, false)))
			{
				//暂不支持其它方式传输
				handleCmd_unsupportedTransport();
				break;
			}
			//设置rtp发送缓冲区大小
			UDP_SetSendBufferSize(m_pCurClientInfo->nRtpSocket, 1024*1024);
			//组织回应信息
			//sprintf_s(m_chSendCmd, 2048,
			sprintf(m_chSendCmd,
				"RTSP/1.0 200 OK\r\n"
				"CSeq: %s\r\n"
				"%s"
				"Transport: RTP/AVP;unicast;destination=%s;source=%s;client_port=%d-%d;server_port=%d-%d\r\n"
				"Session: %08X\r\n\r\n",
				m_chCurrentCSeq,
				strDateHeader,
				m_pCurClientInfo->chClientIP, m_chServerIP , clientRTPPortNum, clientRTCPPortNum,
				m_pCurClientInfo->nSendRtpPort, m_pCurClientInfo->nSendRtcpPort,
				m_pCurClientInfo->chSessionID);
			break;
		}
	case RTP_TCP:
		{
			//设置rtp发送缓冲区大小
			TCP_SetSendBufferSize(m_pCurClientInfo->nClientSock, 1024*1024);
			//组织回应信息
			//sprintf_s(m_chSendCmd, 2048,
			sprintf(m_chSendCmd,
				"RTSP/1.0 200 OK\r\n"
				"CSeq: %s\r\n"
				"%s"
				"Transport: RTP/AVP/TCP;unicast;destination=%s;source=%s;interleaved=%d-%d\r\n"
				"Session: %s\r\n\r\n",
				m_chCurrentCSeq,
				strDateHeader,
				m_pCurClientInfo->chClientIP, m_chServerIP , rtpChannelId, rtcpChannelId,
				m_pCurClientInfo->chSessionID);
		}
		break;
	default:
		{
			//暂不支持其它方式传输
			handleCmd_unsupportedTransport();
		}
		break;
	}
	//释放分配的内存
	delete[] streamingModeString;
}

//创建Session后命令的处理
void CRtspServer::handleCmd_withinSession(char const* cmdName, char const* urlPreSuffix, char const* urlSuffix, char const* fullRequestStr)
{
	//还没有建立连接
	if ((m_pCurClientInfo->chSessionID[0] == '\0') || (m_pCurClientInfo->chVideoName[0] == '\0'))
	{
		handleCmd_notSupported();
		return;
	}
	//检查文件名是否配合
	else if (urlSuffix[0] != '\0' && strcmp(m_pCurClientInfo->chVideoName, urlPreSuffix) == 0)
	{
		//匹配通道编号
		handleCmd_notFound();
		return;
	}
	else if (strcmp(m_pCurClientInfo->chVideoName, urlSuffix) == 0 || (urlSuffix[0] == '\0' && strcmp(m_pCurClientInfo->chVideoName, urlPreSuffix) == 0))
	{

	}
	else if (urlPreSuffix[0] != '\0' && urlSuffix[0] != '\0')
	{
		unsigned int const urlPreSuffixLen = (int)strlen(urlPreSuffix);
		if (strncmp(m_pCurClientInfo->chVideoName, urlPreSuffix, urlPreSuffixLen) == 0
			&& m_pCurClientInfo->chVideoName[urlPreSuffixLen] == '/'
			&& strcmp(&m_pCurClientInfo->chVideoName[urlPreSuffixLen+1], urlSuffix) == 0)
		{
			// FIXME: nothing to do.
		}
		else
		{
			handleCmd_notFound();
			return;
		}
	}
	//没有找到对应的视频流
	else
	{
		handleCmd_notFound();
		return;
	}
	//断开连接命令
	if (strcmp(cmdName, "TEARDOWN") == 0)
	{
		handleCmd_TEARDOWN();
	}
	//开始播放命令
	else if (strcmp(cmdName, "PLAY") == 0)
	{
		handleCmd_PLAY(fullRequestStr);
	}
}

//断开连接命令的回应
void CRtspServer::handleCmd_TEARDOWN()
{
	//通知流节点停止转发
	if(m_pCurClientInfo->nTransFlag)
	{
		//根据传输模式停止对本节点数据的转发
		if(m_pCurClientInfo->mTranMode == RTP_UDP)
		{
			//删除对节点的转发
			((CRtspClient*)(m_pCurClientInfo->pVideoInfo->pRtspClient))->RemoveSubNode(m_pCurClientInfo->nRtpSocket, m_pCurClientInfo->nClientIP, (uint16)(m_pCurClientInfo->nRtpPort));
			//关闭rtp和rtcp通讯端口
			ReleaseRTPandRTCP(m_pCurClientInfo->nRtpSocket, m_pCurClientInfo->nRtcpSocket);
		}
		else if(m_pCurClientInfo->mTranMode == RTP_TCP)
		{
			//删除对节点的转发
			((CRtspClient*)(m_pCurClientInfo->pVideoInfo->pRtspClient))->RemoveSubNode(m_pCurClientInfo->nRtpSocket, (uint8)(m_pCurClientInfo->nRtpChannelId));
		}
		//初始化rtp和rtcp相关信息
		m_pCurClientInfo->nRtpSocket = -1;
		m_pCurClientInfo->nRtcpSocket= -1;
		m_pCurClientInfo->nSendRtpPort= 0;
		m_pCurClientInfo->nSendRtcpPort=0;
		//启动转发标志
		m_pCurClientInfo->nTransFlag = 0;
	}
	//组织RTSP回应数据
	setRTSPResponse("", "200 OK");
	//设置当前客户端活动状态
	m_pCurClientInfo->bActive = false;
}
//开始播放命令的回应
void CRtspServer::handleCmd_PLAY(char const* )
{
	//设置转发信息
	if(m_pCurClientInfo->nTransFlag == 0)
	{
		if(m_pCurClientInfo->mTranMode == RTP_UDP)
		{
			//通知客户端对象 开始转发数据
			((CRtspClient*)(m_pCurClientInfo->pVideoInfo->pRtspClient))->AddSubNode(m_pCurClientInfo->nRtpSocket, m_pCurClientInfo->nClientIP, (uint16)(m_pCurClientInfo->nRtpPort));
			//已开始转发 修改转发标志
			m_pCurClientInfo->nTransFlag = 1;
		}
		else if(m_pCurClientInfo->mTranMode == RTP_TCP)
		{
			//赋值转发参数的rtp套接字
			m_pCurClientInfo->nRtpSocket = m_pCurClientInfo->nClientSock;
			//通知客户端对象 开始转发数据
			((CRtspClient*)(m_pCurClientInfo->pVideoInfo->pRtspClient))->AddSubNode(m_pCurClientInfo->nRtpSocket, (uint8)(m_pCurClientInfo->nRtpChannelId));
			//已开始转发 修改转发标志
			m_pCurClientInfo->nTransFlag = 1;
		}
	}

	//组织时间头信息
	char strDateHeader[200];
	time_t tt = time(NULL);
	strftime(strDateHeader, sizeof strDateHeader, "Date: %a, %b %d %Y %H:%M:%S GMT\r\n", gmtime(&tt));
	//组织RTSP命令的回应数据
	//sprintf_s(m_chSendCmd, 2048,
	sprintf(m_chSendCmd,
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %s\r\n"
		"%s"
		"Session: %s\r\n"
		"\r\n",
		m_chCurrentCSeq,
		strDateHeader,
		m_pCurClientInfo->chSessionID);
}

//解析接收到的请求命令
void CRtspServer::handleRequestBytes(int newBytesRead)
{
	//解析后剩余的数据字节数
	int numBytesRemaining = 0;
	do
	{
		//判断数据长度
		if (newBytesRead < 0 || (unsigned)newBytesRead >= m_pCurClientInfo->nBytesLeft)
		{
			//连接断开 或数据越界 修改客户端状态
			m_pCurClientInfo->bActive = false;
			break;
		}
		//是否找到消息结尾
		bool endOfMsg = false;
		//指向新接收到的数据串
		unsigned char* ptr = (unsigned char*)&m_pCurClientInfo->pRequestBuffer[m_pCurClientInfo->nBytesAlreay];
		// 查找信息结束标志时两个回车换行符: <CR><LF><CR><LF>
		unsigned char *tmpPtr = m_pCurClientInfo->pLastCRLF + 2;
		if (tmpPtr < m_pCurClientInfo->pRequestBuffer) tmpPtr = m_pCurClientInfo->pRequestBuffer;
		while (tmpPtr < &ptr[newBytesRead-1])
		{
			if (*tmpPtr == '\r' && *(tmpPtr+1) == '\n')
			{
				if (tmpPtr - (m_pCurClientInfo->pLastCRLF) == 2)
				{
					endOfMsg = true;
					break;
				}
				m_pCurClientInfo->pLastCRLF = tmpPtr;
			}
			++tmpPtr;
		}
		//修改剩余缓冲区长度和接收到的数据长度
		m_pCurClientInfo->nBytesLeft -= newBytesRead;
		m_pCurClientInfo->nBytesAlreay += newBytesRead;
		//判断是否找到RTSP命令的结束标志
		if (!endOfMsg) break;
		//添加结束符
		m_pCurClientInfo->pRequestBuffer[m_pCurClientInfo->nBytesAlreay] = '\0';

		//解析RTSP命令
		char cmdName[RTSP_PARAM_STRING_MAX];
		char urlPreSuffix[RTSP_PARAM_STRING_MAX];
		char urlSuffix[RTSP_PARAM_STRING_MAX];
		char cseq[RTSP_PARAM_STRING_MAX];
		char sessionIdStr[RTSP_PARAM_STRING_MAX];
		unsigned contentLength = 0;
		m_pCurClientInfo->pLastCRLF[2] = '\0';
		//解析RTSP请求
		bool parseSucceeded = parseRTSPRequestString((char*)m_pCurClientInfo->pRequestBuffer, (unsigned int)(m_pCurClientInfo->pLastCRLF+2 - m_pCurClientInfo->pRequestBuffer),
													cmdName, sizeof cmdName,
													urlPreSuffix, sizeof urlPreSuffix,
													urlSuffix, sizeof urlSuffix,
													cseq, sizeof cseq,
													sessionIdStr, sizeof sessionIdStr,
													contentLength);
		m_pCurClientInfo->pLastCRLF[2] = '\r';
		if (parseSucceeded)
		{
			//RTSP解析成功 确认我们已接收到整个数据包的长度;
			if (ptr + newBytesRead < tmpPtr + 2 + contentLength)
			{
				break;
			}
			//保存命令序号
			m_chCurrentCSeq = cseq;
			//请求可用操作命令
			if (strcmp(cmdName, "OPTIONS") == 0)
			{
				handleCmd_OPTIONS();
			}
			//请求资源描述命令
			else if (strcmp(cmdName, "DESCRIBE") == 0)
			{
				handleCmd_DESCRIBE(urlPreSuffix, urlSuffix, (char const*)m_pCurClientInfo->pRequestBuffer);
			}
			//建立连接命令
			else if (strcmp(cmdName, "SETUP") == 0)
			{
				//生成Session字符串
				sprintf(m_pCurClientInfo->chSessionID, "%08X", m_pCurClientInfo->chSessionID);
				//处理建立连接命令的回应
				handleCmd_SETUP(urlPreSuffix, urlSuffix, (char const*)m_pCurClientInfo->pRequestBuffer);
			}
			//断开连接、开始播放
			else if ((strcmp(cmdName, "TEARDOWN") == 0) || (strcmp(cmdName, "PLAY") == 0))
			{
				//处理播放和断开命令的回应
				handleCmd_withinSession(cmdName, urlPreSuffix, urlSuffix, (char const*)m_pCurClientInfo->pRequestBuffer);
			}
			//其它命令不支持
			else
			{
				//不支持的命令码
				handleCmd_notSupported();
				//2016-1-23,yjx,不支持的命令不需要断开
				//不支持的命令码 需要断开连接
				//m_pCurClientInfo->bActive = false;
			}
		}
		else
		{
			handleCmd_bad();
			//回应解析错误 需要断开连接
			m_pCurClientInfo->bActive = false;
		}
		//发送回应命令
		if(TCP_Send(m_pCurClientInfo->nClientSock, m_chSendCmd, (const int)strlen(m_chSendCmd)) <= 0)
		{
			//发送数据失败, 修改客户端状态
			m_pCurClientInfo->bActive = false;
			break;
		}
		//计算剩余数据长度
		unsigned int requestSize = (unsigned int)(m_pCurClientInfo->pLastCRLF + 4 - m_pCurClientInfo->pRequestBuffer) + contentLength;
		numBytesRemaining = m_pCurClientInfo->nBytesAlreay - requestSize;
		//重置请求缓冲区信息
		m_pCurClientInfo->nBytesAlreay = 0;
		m_pCurClientInfo->nBytesLeft   = m_pCurClientInfo->nRequestBufSize;
		m_pCurClientInfo->pLastCRLF	   = &m_pCurClientInfo->pRequestBuffer[-3];
		//判断是还有剩余数据
		if (numBytesRemaining > 0)
		{
			//删除已解析的请求数据
			memmove(m_pCurClientInfo->pRequestBuffer, &m_pCurClientInfo->pRequestBuffer[requestSize], numBytesRemaining);
			newBytesRead = numBytesRemaining;
		}
	} while (numBytesRemaining > 0);
}
