/*
 * CRtspClient.cpp
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */




#include "Base64.h"
#include "CommTCP.h"
#include "CommUDP.h"
#include "CRtspClient.h"
#include "typedef.h"

#include <arpa/inet.h>

#include <stdio.h>
#include <sys/time.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RTSPCLIENT_DEBUG_LEVEL_1
#define RTSPCLIENT_DEBUG_LEVEL_2

#define RTSP_PARAM_STRING_MAX	200
#define RTP_BUFFER_SIZE			200000

int8 g_chHostName[128] = {0};		//主机名称
int8 g_nHostLen = 0;				//主机名长度

//请求资源可用操作(OPTIONS)
int8 const* chOPTIONfmt =	"OPTIONS %s RTSP/1.0\r\n"
							"CSeq: %d\r\n"
							"%s"
							"User-Agent: %s\r\n"
							"User-Type: %d\r\n"
							"\r\n";
//请求资源描述(DESCRIBE)
int8 const* chDESCRIBEfmt = "DESCRIBE %s RTSP/1.0\r\n"
							"CSeq: %d\r\n"
							"%s"
							"User-Agent: %s\r\n"
							"User-Type: %d\r\n"
							"Accept: application/sdp\r\n"
							"\r\n";
//建立视频流(SETUP)
int8 const* chSETUPfmt1 =	"SETUP %s%s RTSP/1.0\r\n"
							"CSeq: %d\r\n"
							"%s"
							"User-Agent: %s\r\n"
							"User-Type: %d\r\n"
							"Transport: RTP/AVP;unicast;client_port=%d-%d\r\n"
							"\r\n";
//建立视频流(SETUP)
int8 const* chSETUPfmt2 =	"SETUP %s%s RTSP/1.0\r\n"
							"CSeq: %d\r\n"
							"%s"
							"User-Agent: %s\r\n"
							"User-Type: %d\r\n"
							"Transport: RTP/AVP;unicast;destination=%s;client_port=%d-%d\r\n"
							"\r\n";
//建立视频流(SETUP over TCP)
int8 const* chSETUPfmt3 =	"SETUP %s%s RTSP/1.0\r\n"
							"CSeq: %d\r\n"
							"%s"
							"User-Agent: %s\r\n"
							"User-Type: %d\r\n"
							"Transport: RTP/AVP/TCP;unicast;;interleaved=2-3\r\n"
							"\r\n";
//播放视频流(PLAY)
int8 const* chPLAYfmt =		"PLAY %s RTSP/1.0\r\n"
							"CSeq: %d\r\n"
							"%s"
							"User-Agent: %s\r\n"
							"User-Type: %d\r\n"
							"Session: %s\r\n"
							"\r\n";
//停止视频流(TEARDOWN)
int8 const* chTEARDOWNfmt = "TEARDOWN %s RTSP/1.0\r\n"
							"CSeq: %d\r\n"
							"%s"
							"User-Agent: %s\r\n"
							"User-Type: %d\r\n"
							"Session: %s\r\n"
							"\r\n";

//初始化RTP通讯
bool InitiateRTPandRTCP(bool bMultiplexRTCPWithRTP, int32& rtpSock, int32& rtcpSock, char const * localAddr, uint16& rtpPortNum, uint16& rtcpPortNum, bool bNonBlock)
{
	//执行是否成功标志
	bool success = false;
	//分配RTP和RTCP端口
	while (1)
	{
		//创建RTP套接字
		rtpSock = UDP_Open(localAddr, rtpPortNum, bNonBlock);
		if (rtpSock <= 0)
		{
			break;
		}
		//如果RTP和RTCP混合使用一个端口 将套接字保存到RTCP
		if (bMultiplexRTCPWithRTP)
		{
			rtcpSock = rtpSock;
			rtcpPortNum = rtpPortNum;
			success = true;
			break;
		}
		//RTP端口必须为偶数端口
		if ((rtpPortNum&1) != 0)
		{
			UDP_Close(rtpSock);
			rtcpSock = rtpSock = -1;
			rtcpPortNum = rtpPortNum = 0;
			continue;
		}
		//创建RTCP套接字
		rtcpPortNum = rtpPortNum|1;
		rtcpSock = UDP_Open(localAddr, rtcpPortNum, bNonBlock);
		if (rtcpSock > 0)
		{
			success = true;
			break;
		}
		else
		{
			UDP_Close(rtpSock);
			rtcpSock = rtpSock = -1;
			rtcpPortNum = rtpPortNum = 0;
			continue;
		}
	}
	//返回结果
	return success;
}

//释放RTP通讯
void ReleaseRTPandRTCP(int32& rtpSock, int32& rtcpSock)
{
	//rtp和rtcp使用的是相同的套接字
	if((rtpSock > 0) && (rtpSock == rtcpSock))
	{
		rtcpSock = -1;
	}
	//关闭rtp套接字
	if(rtpSock > 0)
	{
		UDP_Close(rtpSock);
		rtpSock = -1;
	}
	//关闭rtcp套接字
	if(rtcpSock > 0)
	{
		UDP_Close(rtcpSock);
		rtcpSock = -1;
	}
}
static char* getLine(char* startOfLine)
{
	// returns the start of the next line, or NULL if none.  Note that this modifies the input string to add '\0' characters.
	for (char* ptr = startOfLine; *ptr != '\0'; ++ptr) {
		// Check for the end of line: \r\n (but also accept \r or \n by itself):
		if (*ptr == '\r' || *ptr == '\n')
		{
			// We found the end of the line
			if (*ptr == '\r')
			{
				*ptr++ = '\0';
				if (*ptr == '\n') ++ptr;
			}
			else
			{
				*ptr++ = '\0';
			}
			return ptr;
		}
	}
	return NULL;
}

static bool checkForHeader(char const* line, char const* headerName, unsigned headerNameLength, char const*& headerParams)
{
	//判断头信息是否一致
	//if (_strnicmp(line, headerName, headerNameLength) != 0)
	if (strncasecmp(line, headerName, headerNameLength) != 0)
	{
		return (false);
	}
	// The line begins with the desired header name.  Trim off any whitespace, and return the header parameters:
	unsigned paramIndex = headerNameLength;
	while (line[paramIndex] != '\0' && (line[paramIndex] == ' ' || line[paramIndex] == '\t')) ++paramIndex;
	// the header is assumed to be bad if it has no parameters
	if (&line[paramIndex] == '\0') return (false);

	//指向参数部分开始
	headerParams = &line[paramIndex];

	return true;
}

//解析传输端口号
bool parseTransportParams(char const* paramsStr, char* sourceAddr, char* destinationAddr, unsigned short& serverPortNum, unsigned char& rtpChannelId, unsigned char& rtcpChannelId)
{
	//服务器RTP端口
	serverPortNum = 0;
	//传输通道id
	rtpChannelId = rtcpChannelId = 0xFF;
	//参数为空 返回失败
	if (paramsStr == NULL) return false;
	//
	bool foundServerPortNum = false;
	unsigned short clientPortNum = 0;
	bool foundClientPortNum = false;
	bool foundChannelIds = false;
	unsigned rtpCid, rtcpCid;

	// Run through each of the parameters, looking for ones that we handle:
	char const* fields = paramsStr;
	char* field = strDupSize(fields);
	while (sscanf(fields, "%[^;]", field) == 1)
	{
		if (sscanf(field, "server_port=%hu", &serverPortNum) == 1)
		{
			foundServerPortNum = true;
		}
		else if (sscanf(field, "client_port=%hu", &clientPortNum) == 1)
		{
			foundClientPortNum = true;
		}
		//else if (_strnicmp(field, "source=", 7) == 0)
		else if (strncasecmp(field, "source=", 7) == 0)
		{
			//strcpy_s(sourceAddr, 64, field+7);
			strcpy(sourceAddr, field+7);
		}
		else if (sscanf(field, "interleaved=%u-%u", &rtpCid, &rtcpCid) == 2)
		{
			rtpChannelId = (unsigned char)rtpCid;
			rtcpChannelId = (unsigned char)rtcpCid;
			foundChannelIds = true;
		}
		//else if (_strnicmp(field, "destination=", 12) == 0)
		else if (strncasecmp(field, "destination=", 12) == 0)
		{
			//strcpy_s(destinationAddr, 64, field+12);
			strcpy(destinationAddr,field+12);
		}

		fields += strlen(field);
		while (fields[0] == ';')
		{
			++fields; // skip over all leading ';' chars
		}
		if (fields[0] == '\0')
		{
			break;
		}
	}
	delete[] field;

	if (foundChannelIds || foundServerPortNum || foundClientPortNum)
	{
		if (foundClientPortNum && !foundServerPortNum)
		{
			serverPortNum = clientPortNum;
		}
		return true;
	}

	return false;
}
static char parseSDPLine(char const* inputLine, char const*& nextLine)
{
	// Begin by finding the start of the next line (if any):
	nextLine = NULL;
	for (char const* ptr = inputLine; *ptr != '\0'; ++ptr)
	{
		if (*ptr == '\r' || *ptr == '\n')
		{
			// We found the end of the line
			++ptr;
			while (*ptr == '\r' || *ptr == '\n') ++ptr;
			nextLine = ptr;
			if (nextLine[0] == '\0') nextLine = NULL; // special case for end
			break;
		}
	}
	// Then, check that this line is a SDP line of the form <char>=<etc>
	// (However, we also accept blank lines in the input.)
	if (inputLine[0] == '\r' || inputLine[0] == '\n')
	{
		return 0;
	}
	if (strlen(inputLine) < 2 || inputLine[1] != '=' || inputLine[0] < 'a' || inputLine[0] > 'z')
	{
		return 0;
	}

	return 1;
}

//构造函数
CRtspClient::CRtspClient(int8* AppInfo,uint32 nType ,const int8 * localIP,int8* chUser, int8* chPwd, bool bOverTCP)
{
	seekFlag = 0;
	//客户端类型
	m_nAppType = nType;
	//客户端程序信息
	m_chAppInfo = new int8[256];
	memset(m_chAppInfo,0x00,256);
	if(AppInfo != NULL && (strlen(AppInfo) < 256))
	{
		//sprintf_s(m_chAppInfo,256,"%s",AppInfo);
		sprintf(m_chAppInfo,"%s",AppInfo);
	}

	//本地IP地址
	m_chLocalIP = new int8[64];
	memset(m_chLocalIP,0x00,64);
	//初始化本地IP地址
	//strcpy_s(m_chLocalIP,64,"127.0.0.1");
	strcpy(m_chLocalIP,"127.0.0.1");
	if ((localIP != NULL) && (strlen(localIP)<60))
	{
		//strcpy_s(m_chLocalIP,64,localIP);
		strcpy(m_chLocalIP,localIP);
	}
	//本地端口号
	m_nLocalPort = 0;

	//连接RTSP服务器的套接字
	m_nRtspSocket = 0;
	//服务器IP地址
	m_chServerIP = new int8[64];
	memset(m_chServerIP,0x00,64);
	//服务器端口
	m_nServerPort = 0;
	//数据发送方的IP和端口
	m_nFromIP  = 0;
	m_nFromPort= 0;

	//视频流名称
	m_chVideoName = new int8[256];
	memset(m_chVideoName,0x00,256);

	//流访问地址
	m_chStreamURL	= new int8[256];
	memset(m_chStreamURL,0x00,256);
	//视频流基地址
	m_chBaseURL = new int8[256];
	memset(m_chBaseURL,0x00,256);

	//用户验证信息
	if((chUser != NULL) && (chPwd != NULL))
	{
		m_pOurAuthenticator = new Authenticator(chUser, chPwd, false);
	}
	else
	{
		m_pOurAuthenticator = new Authenticator("admin", "12345", false);
	}
	//是否基于TCP传输视频流
	m_bOverTCP = bOverTCP;
	//rtsp缓冲区总长度
	m_nResponseBufferSize = 8192;
	//rtsp缓冲区
	m_pResponseBuffer = new int8[m_nResponseBufferSize+1024];
	//rtsp缓冲区占用的长度
	m_nResponseBytesAlreadySeen = 0;
	//rtsp缓冲区剩余的长度
	m_nResponseBufferBytesLeft  = m_nResponseBufferSize;
	//等待回应的命令序号
	m_nWaitCSeq = 0;
	//等待回应的命令码
	m_pWaitCommandName = new int8[64];
	memset(m_pWaitCommandName,0x00,64);
	//视频通道名称
	m_chTrackID = new int8[64];
	memset(m_chTrackID,0x00,64);
	//会话信息缓冲区
	m_chSessionId = new int8[RTSP_PARAM_STRING_MAX];
	memset(m_chSessionId,0x00,RTSP_PARAM_STRING_MAX);
	//视频回话源地址
	m_chSourceAddr = new int8[64];
	memset(m_chSourceAddr,0x00,64);
	m_nSourceAddr = 0;
	//视频回话目的地址
	m_chDestinationAddr = new int8[64];
	memset(m_chDestinationAddr,0x00,64);
	m_nDestinationAddr = 0;
	//源RTP数据端口
	m_nServerRTPPort = 0;
	//源RTCP数据端口
	m_nServerRTCPPort= 0;
	//视频数据接收缓冲区
	m_chRecvBuf = new int8[RTP_BUFFER_SIZE];
	//保存视频通道ID
	m_nRtpChannelId = 2;
	m_nRtcpChannelId= 3;
	//rtp和rtcp是否混合发送
	m_bMultiplexRTCPWithRTP = false;
	//与设备通讯RTP数据的套接字和端口
	m_nRtpSocket = -1;
	m_nRtpPortNum= 0;
	//与设备通讯RTCP数据的套接字和端口
	m_nRtcpSocket= -1;
	m_nRtcpPortNum=0;

	//是否使能发送RTCP报告
	m_bEnableRTCPReports = false;
	//保存RTCP数据的缓冲区
	m_chRTCPBuf = new int8[256];
	//保存RTCP数据的长度
	m_nRTCPLen = 0;
	//RTP包负载类型
	m_nRTPPayloadFormat = 96;
	//TCP接收RTP包的状态
	m_fTCPReadingState = AWAITING_DOLLAR;

	//发送RTCP报告的间隔，2秒 = 2000毫秒
	m_nReportInterval = 2000;

	// linux 使用gettimeofday
	//::GetSystemTimeAsFileTime(&m_sLastTime);
	gettimeofday(&m_sLastTime,NULL);

	//h264的rtp解包对象
	 void *hr = NULL;
	m_pH264RTPunPack = new CH264_Rtp_UnPack(hr);
	//视频队列指针
	m_pVideoQueue = NULL;
	m_pVideoQueueEx = NULL;
	//视频队列元素结构
	m_stDataSrc = NULL;
	//最后接收RTCP包的时间
	gettimeofday(&m_lastRTCP,NULL);
	//rtp数据包序号
	m_nRtpSeq = 0;
	//SR的ntp时间中间32位
	m_nNtpTime= 0;
	//资源描述
	m_nSsrc = 0;
	m_nCsrc = (uint32)(m_chRTCPBuf);

	//转发节点信息
	m_pTranNodeInfo = NULL;
	//节点计数
	m_nNodeCount = 0;

	// linux使用pthread库函数
	//互斥对象用于处理节点添加和删除时的同步
	//InitializeSRWLock(&m_pTranSRWLock);
	pthread_mutex_init(&m_pTranSRWLock,NULL);

	//自定义协议中的包序号
	m_nOutPackNum = 0;
	//用于检测是否接收到了RTP数据
	m_nTickCount = 0;

	//存储统计结果
	//发送数据的长度
	m_nTotalSuccLen		= 0;
	//发送成功的RTP包数
	m_nToalSuccPackNum	= 0;
	//发送失败的RTP包数
	m_nSendFailPackNum	= 0;
	//每一秒的帧数
	m_nFramePerSec		= 0;

	//通讯处理线程Handle
	m_hCommDealThread = 0;
	//通讯处理线程ID
	m_dwCommDealThreadID = 0;
	//通讯处理线程运行状态
	m_nCommDealThreadFlag = 0;

#ifdef RTP_THREAD_FLAG
	//数据转发线程Handle
	m_hTranDataThread	= 0;
	//数据转发线程ID
	m_dwTranDataThreadID= 0;
	//数据转发线程运行状态
	m_nTranDataThreadFlag	= 0;
	//RTP数据队列初始化
	for(int i=0; i<RTP_QUEUESIZE; i++)
	{
		m_pRtpQueue[i].pData = NULL;
		m_pRtpQueue[i].nSize = 0;
		m_pRtpQueue[i].nLen  = 0;
	}
	//RTP队列头指针和尾指针
	m_nRtpQueueHead	= 0;
	m_nRtpQueueTail	= 0;
#endif
	//第一次启动标志
	m_bFirstRun = 1;
	//视频连接断开标志 0-断开 1-发送操作请求 2-发送描述请求 3-发送建立连接请求 4-发送播放请求 5-开始播放
	m_nVideoFlag = 0;
	//视频服务线程的ID
	m_nServiceThreadID = 0;
	//向视频服务发送消息的ID
	m_nServiceMsgID = 0;
	//当前视频编号
	m_nVideoID = 0;
	//当前视频状态
	m_nVideoStatus = 2;

	// 消除警告信息
	this->packLen = 0;
	this->m_fSizeByte1 = 0;
	this->m_fStreamChannelId = 0;
	this->m_fTCPReadSize = 0;
	this->m_nFrameSize = 0;
	this->m_nFrameTs = 0;
	this->m_nFrametype = 0;
	this->m_nFreeSize = 0;
	this->m_nLostFlag = 0;
	this->m_nRecvLen = 0;
	this->m_nRTCPLen = 0;
	this->m_nServerIP = 0;
	this->m_nRTPLen = 0;
	this->m_nSessionTimeout  = 0;

	return;
}
//析构函数
CRtspClient::~CRtspClient()
{
	//停止线程运行
	StopRun();
	//关闭与服务器的RTP和RTCP套接字
	ReleaseRTPandRTCP(m_nRtpSocket, m_nRtcpSocket);

	//关闭TCP通讯端口
	if(m_nRtspSocket > 0)
	{
		TCP_Close(m_nRtspSocket);
		m_nRtspSocket = -1;
	}
	if(m_chAppInfo != NULL)
	{
		delete[] m_chAppInfo;
		m_chAppInfo = NULL;
	}
	if(m_chLocalIP != NULL)
	{
		delete[] m_chLocalIP;
		m_chLocalIP = NULL;
	}
	if(m_chServerIP != NULL)
	{
		delete[] m_chServerIP;
		m_chServerIP = NULL;
	}
	if(m_chVideoName != NULL)
	{
		delete[] m_chVideoName;
		m_chVideoName = NULL;
	}
	if(m_chStreamURL != NULL)
	{
		delete[] m_chStreamURL;
		m_chStreamURL = NULL;
	}
	if(m_chBaseURL != NULL)
	{
		delete[] m_chBaseURL;
		m_chBaseURL = NULL;
	}
	if(m_pOurAuthenticator != NULL)
	{
		delete m_pOurAuthenticator;
		m_pOurAuthenticator = NULL;
	}
	if (m_pResponseBuffer !=NULL)
 	{
 		delete[] m_pResponseBuffer;
 		m_pResponseBuffer = NULL;
 	}
 	if (m_pWaitCommandName !=NULL)
 	{
 		delete[] m_pWaitCommandName;
 		m_pWaitCommandName = NULL;
 	}
	if(m_chTrackID != NULL)
	{
		delete[] m_chTrackID;
		m_chTrackID = NULL;
	}
	if(m_chSessionId != NULL)
	{
		delete[] m_chSessionId;
		m_chSessionId = NULL;
	}
	if(m_chSourceAddr != NULL)
	{
		delete[] m_chSourceAddr;
		m_chSourceAddr = NULL;
	}
	if(m_chDestinationAddr != NULL)
	{
		delete[] m_chDestinationAddr;
		m_chDestinationAddr = NULL;
	}
	if(m_chRecvBuf != NULL)
	{
		delete[] m_chRecvBuf;
		m_chRecvBuf = NULL;
	}
	if(m_chRTCPBuf != NULL)
	{
		delete[] m_chRTCPBuf;
		m_chRTCPBuf = NULL;
	}
	if (m_pH264RTPunPack !=NULL)
	{
		delete m_pH264RTPunPack;
		m_pH264RTPunPack = NULL;
	}

	//互斥访问
	//AcquireSRWLockExclusive(&m_pTranSRWLock);
	pthread_mutex_lock(&m_pTranSRWLock);

	//数据转发
	for(STranNodeInfo* pNodeInfo=m_pTranNodeInfo; pNodeInfo != NULL;)
	{
		//临时保存当前节点信息
		STranNodeInfo* pCurInfo = pNodeInfo;
		//获取下一个节点
		pNodeInfo = (STranNodeInfo*)(pNodeInfo->pNextObj);
		//删除当前节点信息
		delete[] pCurInfo; pCurInfo = NULL;
	}

	//释放互斥同步对象
	//ReleaseSRWLockExclusive(&m_pTranSRWLock);
	pthread_mutex_unlock(&m_pTranSRWLock);

#ifdef RTP_THREAD_FLAG
	//释放RTP数据队列
	for(int i=0; i<RTP_QUEUESIZE; i++)
	{
		if(m_pRtpQueue[i].pData == NULL)
			continue;
		delete[] m_pRtpQueue[i].pData;
		m_pRtpQueue[i].pData = NULL;
	}
	//RTP队列头指针和尾指针
	m_nRtpQueueHead	= 0;
	m_nRtpQueueTail	= 0;
#endif
	return;
}
/*
函数功能：	初始化模块
函数接口：	int8 Init(const int8* chServerIP, const uint16 nServerPort);
入口参数：	chServerIP	服务器地址；
			nServerPort	服务器端口号；
出口参数：	无；
返回值：	0：成功，1：失败；
*/
int8 CRtspClient::Init(const int8* chServerIP, const uint16 nServerPort)
{
	//判断输入参数合法性
	if((chServerIP == NULL) || (strlen(chServerIP) > 60))
	{
		return 1;
	}
	if((nServerPort <= 0) || (nServerPort > 65535))
	{
		return 1;
	}
	//保存P2P服务器IP地址和端口
	//strcpy_s(m_chServerIP,64,chServerIP);
	strcpy(m_chServerIP, chServerIP);

	m_nServerIP = inet_addr(m_chServerIP);
	m_nServerPort = nServerPort;

	return  0;
}

/*
函数功能：	初始化模块
函数接口：	int8 Init(const int8* chName, const int8* chStreamURL, CQueue* fQueue);
入口参数：	chName			流名称
			chStreamURL		流地址
			fQueue			数据队列指针
出口参数：	无；
返回值：	0：成功，1：失败；
*/
int8 CRtspClient::Init(const int8* chName,const int8* chStreamURL, CQueue* fQueue)
{
	//判断输入参数合法性
	if((chName == NULL) || (strlen(chName) > 255))
	{
		return 1;
	}
	if((chStreamURL == NULL) || (strlen(chStreamURL) > 255))
	{
		return 1;
	}
	//流名称
	//strcpy_s(this->m_chVideoName,256,chName);
	strcpy(this->m_chVideoName,chName);

	//保存流访问地址
	//strcpy_s(this->m_chStreamURL,256,chStreamURL);
	strcpy(this->m_chStreamURL,chStreamURL);

	//strcpy_s(this->m_chBaseURL,256,chStreamURL);
	strcpy(this->m_chBaseURL,chStreamURL);

	//数据队列
	this->m_pVideoQueue = fQueue;

	return  0;
}
/*
函数功能：	初始化模块
函数接口：	int8 Init(int8* chServerIP,uint16 nServerPort, int8* chName,int8* chStreamURL, CQueue* fQueue);
入口参数：	chServerIP		服务器地址；
			nServerPort		服务器端口号；
			chName			流名称
			chStreamURL		流地址
			fQueue			数据队列指针
出口参数：	无；
返回值：	0：成功，1：失败；
*/
int8 CRtspClient::Init(const int8* chServerIP, const uint16 nServerPort, const int8* chName, const int8* chStreamURL, CQueue* fQueue)
{
	//判断输入参数合法性
	if((chServerIP == NULL) || (strlen(chServerIP) > 60))
	{
		return 1;
	}
	if((nServerPort <= 0) || (nServerPort > 65535))
	{
		return 1;
	}
	if((chName == NULL) || (strlen(chName) > 255))
	{
		return 1;
	}
	if((chStreamURL == NULL) || (strlen(chStreamURL) > 255))
	{
		return 1;
	}
	//保存P2P服务器IP地址和端口
	//strcpy_s(m_chServerIP,64,chServerIP);
	strcpy(m_chServerIP,chServerIP);

	m_nServerIP = inet_addr(m_chServerIP);
	m_nServerPort = nServerPort;
	//流名称
	//strcpy_s(this->m_chVideoName,256,chName);
	strcpy(this->m_chVideoName,chName);
	//保存流访问地址
	//strcpy_s(this->m_chStreamURL,256,chStreamURL);
	strcpy(this->m_chStreamURL,chStreamURL);
	//strcpy_s(this->m_chBaseURL,256,chStreamURL);
	strcpy(this->m_chBaseURL,chStreamURL);
	//数据队列
	this->m_pVideoQueue = fQueue;

	return  0;
}
/*
函数功能：	初始化队列
函数接口：	int8 InitQueue(unsigned int uMemSize, unsigned int uMaxDataSize, unsigned int uMaxExtaraInfoSize);
入口参数：	uMemSize			队列内存大小；
			uMaxDataSize		数据最大长度；
			uMaxExtaraInfoSize	扩展信息最大长度
出口参数：	无；
返回值：	0：成功，1：失败；
*/
int8 CRtspClient::InitQueue(unsigned int uMemSize, unsigned int uMaxDataSize, unsigned int uMaxExtaraInfoSize)
{
	//创建队列对象
	m_pVideoQueueEx = new CMyQueue();
	//判断对象是否创建成功成功
	if(m_pVideoQueueEx == NULL)
	{
		return (1);
	}
	//初始化队列内存
	if(0 != m_pVideoQueueEx->queInit(uMemSize, uMaxDataSize, uMaxExtaraInfoSize))
	{
		//队列初始化失败，销毁队列对象
		delete m_pVideoQueueEx;m_pVideoQueueEx=NULL;
		//返回失败
		return (1);
	}
	//返回成功
	return 0;
}
/*
函数功能：设置状态上报参数
函数接口：void SetReportParam(int32 nVideoID, DWORD nThreadID, UINT nMsgID);
入口参数：	nVideoID	视频资源ID
			nThreadID	服务线程ID；
			nMsgID		发送的消息ID；
返回值：	无
*/
void CRtspClient::SetReportParam(int32 nVideoID, DWORD nThreadID, UINT nMsgID)
{
	//视频资源ID
	m_nVideoID = (uint16)nVideoID;
	//视频服务线程的ID
	m_nServiceThreadID = nThreadID;
	//向视频服务发送消息的ID
	m_nServiceMsgID = nMsgID;
}
/*
函数功能：	请求资源访问地址
函数接口：	int8 GetStreamURL(int8* chName, int8* chVideoAddr);
入口参数：	chName		视频流名称；
出口参数：	chVideoAddr	视频流访问地址；
返回值：	0：成功，1：失败。
函数功能：	获取视频流数据访问地址
*/
int8 CRtspClient::GetStreamURL(int8* , int8* chVideoAddr)
{
	//判断输出指针是否为空
	if(chVideoAddr == NULL)
	{
		return 1;
	}
	//判断流地址长度
	if((this->m_chBaseURL == NULL) || (strlen(this->m_chBaseURL) > 127))
	{
		return 1;
	}
	//给输出参数赋值
	//strcpy_s(chVideoAddr,128,this->m_chBaseURL);
	strcpy(chVideoAddr,this->m_chBaseURL);
	//结束 返回失败
	return 1;
}

//获取视频数据
int32 CRtspClient::GetVideoData( StQueueData* stVideoData)
{
	//旧版本视频队列
	if(m_pVideoQueue != NULL)
	{
		//从队列中获取数据 并返回执行结果
		return m_pVideoQueue->queGet(stVideoData);
	}
	//新版本视频队列
	if(m_pVideoQueueEx != NULL)
	{
		StMyQueueData* tempVideoData;
		//获取视频数据
		if(0 > m_pVideoQueueEx->queGet(&tempVideoData, true))
		{
			return (-1);
		}
		else
		{
			//视频数据
			stVideoData->dataLen = tempVideoData->dataLen;
			stVideoData->pVoidData = tempVideoData->pVoidData;
			//扩展信息
			stVideoData->lengthOFExtaraInfo = tempVideoData->lengthOFExtaraInfo;
			stVideoData->pVoidExtraInfo = tempVideoData->pVoidExtraInfo;
			//返回成功
			return (stVideoData->dataLen);
		}
	}
	//返回失败
	return (-1);
}
/*
函数功能：请求可用操作
函数接口：int8 sendOPTIONCmd();
入口参数：	无；
出口参数：	无；
返回值：	0：发送成功，1：发送失败；
函数功能：	向服务器请求视频流可用操作。
*/
int8 CRtspClient::sendOPTIONCmd()
{
	//当前状态
	m_nVideoFlag = 0;
	//如果没有连接服务器 先连接服务器
	if(m_nRtspSocket<=0)
	{
		m_nLocalPort = 0;
		m_nRtspSocket = TCP_Connect((int*)&m_nLocalPort,m_chServerIP,(int)m_nServerPort);
		//如果是TCP传输视频数据 设置TCP接收缓冲区
		if((m_nRtspSocket>0) && (m_bOverTCP))
		{
			TCP_SetRecvBufferSize(m_nRtspSocket, 1024*1024);
		}
	}
	//连接服务器失败
	if(m_nRtspSocket <=0)
	{
		//返回失败
		return 1;
	}
	//分配命令缓冲区空间
	int8* chSendCMD = new int8[512];
	//缓冲区分配失败
	if(chSendCMD == NULL) return 1;

	//获取验证信息
	char* authenticatorStr = createAuthenticatorString("OPTION", m_chBaseURL);
	//保存发送的命令码
	//strcpy_s(m_pWaitCommandName,32,"OPTIONS");
	strcpy(m_pWaitCommandName,"OPTIONS");
	//组织RTSP命令
	//sprintf_s(chSendCMD,512,chOPTIONfmt,m_chStreamURL,++m_nWaitCSeq,authenticatorStr,m_chAppInfo,m_nAppType);
	sprintf(chSendCMD,chOPTIONfmt,m_chStreamURL,++m_nWaitCSeq,authenticatorStr,m_chAppInfo,m_nAppType);
	//释放用户验证的内存
	delete[] authenticatorStr;
#ifdef _DEBUG
#ifdef RTSPCLIENT_DEBUG_LEVEL_1
	TRACE("Client 发送RTSP命令\r\n%s", chSendCMD);
#endif
#endif
	//发送RTSP命令
	if(TCP_Send(m_nRtspSocket,chSendCMD,(int)strlen(chSendCMD)) <= 0)
	{
		//释放内存空间
		delete[] chSendCMD;chSendCMD=NULL;
		//发送失败时，关闭连接
		TCP_Close(m_nRtspSocket); m_nRtspSocket = -1;
		//返回失败
		return 1;
	}
	//释放内存空间
	delete[] chSendCMD;chSendCMD=NULL;
	//rtsp缓冲区占用的长度
	m_nResponseBytesAlreadySeen = 0;
	//rtsp缓冲区剩余的长度
	m_nResponseBufferBytesLeft  = m_nResponseBufferSize;
	//当前状态
	m_nVideoFlag = 1;
	//结束 返回成功
	return 0;
}

/*
函数功能：请求资源描述
函数接口：int8 sendDESCRIBECmd();
入口参数：	无；
出口参数：	无；
返回值：	0：发送成功，1：发送失败；
函数功能：	向服务器请求视频流描述信息，获取视频流描述信息
*/
int8 CRtspClient::sendDESCRIBECmd()
{
	//如果没有连接服务器 先连接服务器
	if(m_nRtspSocket <=0 )
	{
		m_nLocalPort = 0;
		m_nRtspSocket = TCP_Connect((int*)&m_nLocalPort,m_chServerIP,(int)m_nServerPort);
		//如果是TCP传输视频数据 设置TCP接收缓冲区
		if((m_nRtspSocket>0) && (m_bOverTCP))
		{
			TCP_SetRecvBufferSize(m_nRtspSocket, 1024*1024);
		}
	}
	//连接失败
	if(m_nRtspSocket <= 0)
	{
		return 1;
	}
	//当前状态
	m_nVideoFlag = 1;
	//分配命令缓冲区空间
	int8* chSendCMD = new int8[512];
	//缓冲区分配失败
	if(chSendCMD == NULL) return 1;

	//获取验证信息
	char* authenticatorStr = createAuthenticatorString("DESCRIBE", m_chBaseURL);
	//TRACE("the authenticatorStr is %s\r\n",authenticatorStr);
	printf("the authenticatorStr is %s\r\n",authenticatorStr);
	//保存发送的命令码
	//strcpy_s(m_pWaitCommandName,32,"DESCRIBE");
	strcpy(m_pWaitCommandName,"DESCRIBE");
	//连接成功后组织协议数据
	//sprintf_s(chSendCMD,512,chDESCRIBEfmt,m_chStreamURL,++m_nWaitCSeq,authenticatorStr,m_chAppInfo,m_nAppType);
	sprintf(chSendCMD,chDESCRIBEfmt,m_chStreamURL,++m_nWaitCSeq,authenticatorStr,m_chAppInfo,m_nAppType);
	//释放用户验证的内存
	delete[] authenticatorStr;
#ifdef _DEBUG
#ifdef RTSPCLIENT_DEBUG_LEVEL_1
	TRACE("Client 发送RTSP命令\r\n%s", chSendCMD);
#endif
#endif
	//调用commTCP中TCP_Send接口函数发送数据
	if(TCP_Send(m_nRtspSocket,chSendCMD,(const int)strlen(chSendCMD)) <= 0) {
		//释放内存空间
		delete[] chSendCMD;chSendCMD=NULL;
		//发送失败时，关闭连接
		TCP_Close(m_nRtspSocket); m_nRtspSocket = -1;
		//当前状态
		m_nVideoFlag = 0;
		//返回失败
		return 1;
	}
	//释放内存空间
	delete[] chSendCMD;chSendCMD=NULL;
	//rtsp缓冲区占用的长度
	m_nResponseBytesAlreadySeen = 0;
	//rtsp缓冲区剩余的长度
	m_nResponseBufferBytesLeft  = m_nResponseBufferSize;
	//视频当前状态
	m_nVideoFlag = 2;
	//结束 返回成功
	return 0;
}

//测试发送的
static byte srdata[] = {0xCE, 0xFA, 0xED, 0xFE };
/*
函数功能：建立连接
函数接口：int8 sendSETUPCmd();
入口参数：	无
出口参数：	无；
返回值：	0：发送成功，1：发送失败；
函数功能：	向服务器发送建立连接请求
*/
int8 CRtspClient::sendSETUPCmd()
{
	//如果没有连接服务器 先连接服务器
	if(m_nRtspSocket <=0 )
	{
		m_nLocalPort= 0;
		m_nRtspSocket = TCP_Connect((int*)&m_nLocalPort,m_chServerIP,(int)m_nServerPort);
		//如果是TCP传输视频数据 设置TCP接收缓冲区
		if((m_nRtspSocket>0) && (m_bOverTCP))
		{
			TCP_SetRecvBufferSize(m_nRtspSocket, 1024*1024);
		}
	}
	//连接失败
	if(m_nRtspSocket <=0)
		return 1;
	//如果是UDP通讯方式, 打开本地通讯端口
	if(m_bOverTCP == false)
	{
		//初始化与服务器的RTP和RTCP套接字
		if((m_nRtpSocket <= 0) && (false == InitiateRTPandRTCP(m_bMultiplexRTCPWithRTP, m_nRtpSocket, m_nRtcpSocket, m_chLocalIP, m_nRtpPortNum, m_nRtcpPortNum, true)))
		{
			m_nRtpSocket = m_nRtcpSocket = -1;
			m_nRtpPortNum= m_nRtcpPortNum= 0;
			return 1;
		}
		//设置rtp接收缓冲区大小
		UDP_SetRecvBufferSize(m_nRtpSocket, 1024*1024);
	}
	//分配命令缓冲区空间
	int8* chSendCMD = new int8[512];
	//缓冲区分配失败
	if(chSendCMD == NULL)
	{
		return 1;
	}
	//创建H264解包类
	if (m_pH264RTPunPack ==NULL)
	{
		//HRESULT hr;
		void * hr = NULL;
		m_pH264RTPunPack = new CH264_Rtp_UnPack(hr,(unsigned char)m_nRTPPayloadFormat);
	}
	else if(m_nRTPPayloadFormat != m_pH264RTPunPack->m_H264PayLoadType)
	{
		//HRESULT hr;
		void* hr = NULL;
		//释放原来RTP解包类对象
		if (m_pH264RTPunPack !=NULL)
		{
			delete m_pH264RTPunPack;
			m_pH264RTPunPack = NULL;
		}
		//重新创建RTP解包实例
		m_pH264RTPunPack = new CH264_Rtp_UnPack(hr,(unsigned char)m_nRTPPayloadFormat);
	}
	//处理流地址和通道信息
	if(m_chTrackID[0] == '\0')
	{
		//去掉最后的分隔符
		if(m_chBaseURL[strlen(m_chBaseURL)-1] == '/')
			m_chBaseURL[strlen(m_chBaseURL)-1] = '\0';
	}
	else
	{
		if((m_chTrackID[0] != '/') && (m_chBaseURL[strlen(m_chBaseURL)-1] != '/'))
		{
			char chTemp[50];
			//strcpy_s(chTemp,50,m_chTrackID);
			strcpy(chTemp,m_chTrackID);
			//sprintf_s(m_chTrackID,49,"/%s",chTemp);
			sprintf(m_chTrackID,"/%s",chTemp);
		}
	}

	//获取验证信息
	char* authenticatorStr = createAuthenticatorString("SETUP", m_chBaseURL);
	//保存发送的命令码
	//strcpy_s(m_pWaitCommandName,32,"SETUP");
	strcpy(m_pWaitCommandName,"SETUP");
	//组织命令字符串 根据是否为TCP传输视频流 组织不同的请求命令
	if(m_bOverTCP)
	{
		//sprintf_s(chSendCMD,512,chSETUPfmt3,m_chBaseURL,m_chTrackID,++m_nWaitCSeq,authenticatorStr,m_chAppInfo,m_nAppType);
		sprintf(chSendCMD,chSETUPfmt3,m_chBaseURL,m_chTrackID,++m_nWaitCSeq,authenticatorStr,m_chAppInfo,m_nAppType);
	} else
	{
		if (strcmp(m_chLocalIP,"127.0.0.1")==0)
		{
			//sprintf_s(chSendCMD,512,chSETUPfmt1,m_chBaseURL,m_chTrackID,++m_nWaitCSeq,authenticatorStr,m_chAppInfo,m_nAppType,m_nRtpPortNum, m_nRtcpPortNum);
			sprintf(chSendCMD,chSETUPfmt1,m_chBaseURL,m_chTrackID,++m_nWaitCSeq,authenticatorStr,m_chAppInfo,m_nAppType,m_nRtpPortNum, m_nRtcpPortNum);
		}
		else
		{
			//sprintf_s(chSendCMD,512,chSETUPfmt2,m_chBaseURL,m_chTrackID,3,++m_nWaitCSeq,authenticatorStr,m_nAppType,m_chLocalIP,m_nRtpPortNum, m_nRtcpPortNum);
			sprintf(chSendCMD,chSETUPfmt2,m_chBaseURL,m_chTrackID,3,++m_nWaitCSeq,authenticatorStr,m_nAppType,m_chLocalIP,m_nRtpPortNum, m_nRtcpPortNum);
		}
	}
#ifdef _DEBUG
#ifdef RTSPCLIENT_DEBUG_LEVEL_1
	TRACE("Client 发送RTSP命令\r\n%s", chSendCMD);
#endif
#endif
	//释放用户验证的内存
	delete[] authenticatorStr;
	//调用commTCP中TCP_Send接口函数发送数据
	if(TCP_Send(m_nRtspSocket,chSendCMD,(const int)strlen(chSendCMD)) <= 0)
	{
		//释放内存空间
		delete[] chSendCMD;chSendCMD=NULL;
		//发送失败时，关闭连接
		TCP_Close(m_nRtspSocket); m_nRtspSocket = -1;
		//当前状态
		m_nVideoFlag = 0;
		return 1;//返回失败
	}
	//释放内存空间
	delete[] chSendCMD;chSendCMD=NULL;
	//复位缓存的RTSP数据缓冲区
	m_nResponseBytesAlreadySeen = 0;
	m_nResponseBufferBytesLeft = m_nResponseBufferSize;
	//视频当前状态
	m_nVideoFlag = 3;
	//结束 返回成功
	return 0;
}

/*
函数功能：开始播放
函数接口：int8 sendPLAYCmd();
入口参数：	无；
出口参数：	无；
返回值：	0：发送成功，1：发送失败；
函数功能：	向服务器发送开始播放请求
*/
int8 CRtspClient::sendPLAYCmd()
{
	//判断是否已连接RTSP服务器连接是否打开
	if(m_nRtspSocket <=0 )
		return 1;
	//分配命令缓冲区空间
	int8* chSendCMD = new int8[512];
	//缓冲区分配失败
	if(chSendCMD == NULL) return 1;

	//获取验证信息
	char* authenticatorStr = createAuthenticatorString("PLAY", m_chBaseURL);
	//保存发送的命令码
	//strcpy_s(m_pWaitCommandName,32,"PLAY");
	strcpy(m_pWaitCommandName,"PLAY");
	//连接成功后组织协议数据
	//sprintf_s(chSendCMD,512,chPLAYfmt,m_chBaseURL,++m_nWaitCSeq,authenticatorStr,m_chAppInfo,m_nAppType,m_chSessionId);
	sprintf(chSendCMD,chPLAYfmt,m_chBaseURL,++m_nWaitCSeq,authenticatorStr,m_chAppInfo,m_nAppType,m_chSessionId);
	//释放用户验证的内存
	delete[] authenticatorStr;
#ifdef _DEBUG
#ifdef RTSPCLIENT_DEBUG_LEVEL_1
	TRACE("Client 发送RTSP命令\r\n%s", chSendCMD);
#endif
#endif
	//调用commTCP中TCP_Send接口函数发送数据
	if(TCP_Send(m_nRtspSocket,chSendCMD,(const int)strlen(chSendCMD)) <= 0)
	{
		//释放内存空间
		delete[] chSendCMD;chSendCMD=NULL;
		//发送失败时，关闭连接
		TCP_Close(m_nRtspSocket); m_nRtspSocket = -1;
		//当前状态
		m_nVideoFlag = 0;
		return 1;//返回失败
	}
	//释放内存空间
	delete[] chSendCMD;chSendCMD=NULL;
	//rtsp缓冲区占用的长度
	m_nResponseBytesAlreadySeen = 0;
	//rtsp缓冲区剩余的长度
	m_nResponseBufferBytesLeft  = m_nResponseBufferSize;
	//视频当前状态
	m_nVideoFlag = 4;
	//UDP通讯模式 测试数据发送
	if(m_bOverTCP == false)
	{
		UDP_Send(m_nRtpSocket, m_nSourceAddr, m_nServerRTPPort, (const char *)srdata, 4);
		UDP_Send(m_nRtcpSocket,  m_nSourceAddr, m_nServerRTCPPort, (const char *)srdata, 4);
	}
	//结束 返回成功
	return 0;
}

/*
函数功能：断开连接
函数接口：int8 sendTEARDOWNCmd();
入口参数：	无；
出口参数：	无；
返回值：	0：发送成功，1：发送失败；
函数功能：	向服务器发送终止流传输请求
*/
int8 CRtspClient::sendTEARDOWNCmd()
{
	//判断是否需要发送离开报告
	if((m_nServerRTCPPort != 0) && (m_nRtcpSocket > 0))
	{
		SendBYE();
	}
	//判断服务器连接是否打开
	if(m_nRtspSocket <=0 )
	{
		return 1;
	}
	//分配命令缓冲区空间
	int8*  chSendCMD = new int8[512];
	//缓冲区分配失败
	if(chSendCMD == NULL) return 1;

	//获取验证信息
	char* authenticatorStr = createAuthenticatorString("TEARDOWN", m_chBaseURL);
	//保存发送的命令码
	//strcpy_s(m_pWaitCommandName,32,"TEARDOWN");
	strcpy(m_pWaitCommandName,"TEARDOWN");
	//连接成功后组织协议数据
	sprintf(chSendCMD,chTEARDOWNfmt,m_chBaseURL,++m_nWaitCSeq,authenticatorStr,m_chAppInfo,m_nAppType,m_chSessionId);
	//释放用户验证的内存
	delete[] authenticatorStr;
#ifdef _DEBUG
#ifdef RTSPCLIENT_DEBUG_LEVEL_1
	TRACE("Client 发送RTSP命令\r\n%s", chSendCMD);
#endif
#endif
	//调用commTCP中TCP_Send接口函数发送数据
	if(TCP_Send(m_nRtspSocket,chSendCMD,(const int)strlen(chSendCMD)) <= 0)
	{
		//释放内存空间
		delete[] chSendCMD;chSendCMD=NULL;
		//发送失败时，关闭连接
		TCP_Close(m_nRtspSocket); m_nRtspSocket = -1;
		return 1;//返回失败
	}
	//释放内存空间
	delete[] chSendCMD;chSendCMD=NULL;
	//结束 返回成功
	return 0;
}

/*
函数功能：处理线程
函数接口：static DWORD CommDealThread(LPVOID pParam);
入口参数：	pParam		线程启动参数，RTSP客户端实例。
出口参数：	无；
返回值：	0：线程正常退出，1：线程异常退出。
函数功能：	通讯处理线程，接收RTSP协议和视频数据
*/
void* CRtspClient::CommDealThread(void* pParam)
{
	//定义RTSP客户端指针指向传入参数
	CRtspClient* pClient = (CRtspClient*)pParam;
	//是否退出
	while(pClient->m_nCommDealThreadFlag)
	{
		//接收处理TCP数据
		pClient->DealTCPData();
		//接收处理UDP数据
		pClient->DealUDPData();
	}
	//关闭线程句柄
	//CloseHandle(pClient->m_hCommDealThread);
	// FIXME： pthread如何关闭句柄？

	pClient->m_hCommDealThread = NULL;
	return 0;//线程正常退出
}

#ifdef RTP_THREAD_FLAG
/*
函数功能：数据转发线程
函数接口：static DWORD TranDataThread(LPVOID pParam);
入口参数：	pParam		线程启动参数，RTSP客户端实例。
出口参数：	无；
返回值：	0：线程正常退出，1：线程异常退出。
函数功能：	数据转发线程 通过此线程将数据转发到客户端
*/
//DWORD __stdcall  CRtspClient::TranDataThread(LPVOID pParam)
void*  CRtspClient::TranDataThread(void* pParam)
{
	//定义RTSP客户端指针指向传入参数
	CRtspClient* pClient = (CRtspClient*)pParam;
	//是否退出
	while(pClient->m_nTranDataThreadFlag)
	{
		//转发RTP数据
		pClient->TranDataFunc();
	}
	//关闭线程句柄
	//CloseHandle(pClient->m_hTranDataThread);
	//FIXME：pthread如何关闭句柄

	pClient->m_hTranDataThread = NULL;
	return 0;//线程正常退出
}
#endif
/*
函数功能：	启动模块
函数接口：	int8 StartRun()
入口参数：	chName		视频流名称；
			chVideoAddr	视频流访问地址；
			fQueue		视频数据队列；
出口参数：	无；
返回值：	0：成功，1：失败
函数功能：	启动处理线程，发送建立连接请求，和视频服务器建立视频流连接，下载数据
*/
int8 CRtspClient::StartRun()
{
	//如果模块已启动直接返回
	if(m_nCommDealThreadFlag > 0)
		return 0;
	//第一次启动标志
	m_bFirstRun = 1;
	//帧跳检测标志
	m_nTickCount = 0;
	/*int i = sendDESCRIBECmd();
	if (i == 1)
	{
		return 1;
	}*/
	//得到当前时间
	ftime(&m_lasttb);
#ifdef RTP_THREAD_FLAG
	//节点类型为2时才处理转发
	if(m_nAppType == 2){
		//数据转发线程运行状态
		m_nTranDataThreadFlag = 1;
//		//启动数据转发线程
//		m_hTranDataThread = CreateThread(
//										NULL,					//安全属性
//										0,						//堆栈大小
//										&TranDataThread,		//线程函数
//										this,					//参数
//										0,						//马上运行
//										&m_dwTranDataThreadID	//线程id
//										);

		m_dwTranDataThreadID = 1;
		pthread_create(
				&this->m_hTranDataThread,
				NULL,
				TranDataThread,
				(void*)this
		);

		//线程启动失败
		if(m_hTranDataThread == NULL)
		{
			m_dwTranDataThreadID  = 0;
			m_nTranDataThreadFlag = 0;
		}
	}
#endif
	//线程运行状态
	m_nCommDealThreadFlag = 1;
	//启动线程
//	m_hCommDealThread = CreateThread(
//								NULL,					//安全属性
//								0,						//堆栈大小
//								&CommDealThread,		//线程函数
//								this,					//参数
//								0,						//马上运行
//								&m_dwCommDealThreadID	//线程id
//								);
	m_dwCommDealThreadID = 2;
	pthread_create(
			&this->m_hCommDealThread,
			NULL,
			CommDealThread,
			this
	);

	//线程启动成功
	if(m_hCommDealThread != NULL)
	{
		return 0;
	}
	//线程启动失败 将标志修改为0
	m_nCommDealThreadFlag = 0;
	m_dwCommDealThreadID = 0;

	return 1;
}
/*****************************************************************
函数功能：	停止模块
函数接口：	int8 StopRun()
入口参数：	无；
出口参数：	无；
返 回 值：	0：成功，1：失败
函数功能：	停止视频数据下载，断开连接
******************************************************************/
int8 CRtspClient::StopRun()
{
#ifdef RTP_THREAD_FLAG
	//停止数据转发线程
	if(m_hTranDataThread != NULL)
	{
		do
		{
			//停止线程运行
			m_nTranDataThreadFlag = 0;
			//延时10毫秒
			//Sleep(10);
			usleep(10*1000);

		}while(m_hTranDataThread != 0);
		//线程ID赋值为0
		m_dwTranDataThreadID = 0;
	}
#endif
	//停止通讯处理线程
	if(m_hCommDealThread != NULL)
	{
		do
		{
			//停止线程运行
			m_nCommDealThreadFlag = 0;
			//延时10毫秒
			//Sleep(10);
			usleep(10*1000);

		}while(m_hCommDealThread != 0);
		//线程ID赋值为0
		m_dwCommDealThreadID = 0;
	}
	//停止视频下载
	if(m_nVideoFlag != 0)
	{
		//发送断开命令
		sendTEARDOWNCmd();
		//关闭TCP连接
		if(m_nRtspSocket > 0)
		{
			TCP_Close(m_nRtspSocket);
			m_nRtspSocket = -1;
		}
		m_nVideoFlag = 0;
	}
	return 0;
}
//得到视频流状态
int8 CRtspClient::GetVideoStatus()
{
	return m_nVideoFlag;
}
//接收处理TCP数据
int8 CRtspClient::DealTCPData()
{
	//用于保存时间差
	DWORD subTime;
	//发送当前视频状态到服务程序
	if(this->m_nVideoFlag < 4)
	{
		//发送通知消息 通知服务视频流状态
		if((m_nServiceThreadID > 0) && (this->m_nVideoStatus != 0)) {

			// FIXME: linux下如何实现
//			//消息参数
//			WPARAM wParam = (WPARAM)((0x02<<28)|(this->m_nVideoID<<16));
//			LPARAM lParam = (LPARAM)0;
//			this->m_nVideoStatus = 0;
//			::PostThreadMessage(m_nServiceThreadID, m_nServiceMsgID, wParam, lParam);
			printf("linux下如何实现: ::PostThreadMessage(m_nServiceThreadID, m_nServiceMsgID, wParam, lParam);\r\n");
		}
	}
	else
	{
		//发送通知消息 通知服务视频流状态
		if((m_nServiceThreadID > 0) && (this->m_nVideoStatus != 1))
		{

			// FIXME: linux下如何实现
//			//消息参数
//			WPARAM wParam = (WPARAM)((0x01<<28)|(this->m_nVideoID<<16));
//			LPARAM lParam = (LPARAM)0;
//			this->m_nVideoStatus = 1;
//			::PostThreadMessage(m_nServiceThreadID, m_nServiceMsgID, wParam, lParam);
			printf("linux下如何实现: ::PostThreadMessage(m_nServiceThreadID, m_nServiceMsgID, wParam, lParam);\r\n");
		}
	}
	//判断连接状态
	if(m_nRtspSocket <= 0)
	{
		//得到当前时间
		ftime(&m_curtb);
		//获取时间间隔
		subTime = (int32)((m_curtb.time - m_lasttb.time)*1000 + (m_curtb.millitm-m_lasttb.millitm));
		//判断是否到重连时间
		if((m_bFirstRun == 1)|| (subTime > RECONNECTTIME))
		{
			//发送请求可用操作命令
			//sendOPTIONCmd();
			//发送请求资源描述命令
			sendDESCRIBECmd();
			//第一次启动标志
			m_bFirstRun = 0;
			//保存当前时间
			ftime(&m_lasttb);
			//帧跳检测标志
			m_nTickCount = 0;
		}
		//添加延时处理
		//Sleep(10);
		usleep(10*1000);

		return 1;
	}
	//接收数据长度清零
	m_nRecvLen = 0;
	//判断是否接受RTSP命令
	if((this->m_bOverTCP == false) || (this->m_nVideoFlag != 5))
	{
		//TRACE("the m_nVideoFlag is %d\r\n",this->m_nVideoFlag);
		// FIXME
		//printf("the m_nVideoFlag is %d\r\n",this->m_nVideoFlag);

		//接受RTSP命令
		m_nRecvLen = TCP_Recv(m_nRtspSocket,(char*)&m_pResponseBuffer[m_nResponseBytesAlreadySeen], m_nResponseBufferBytesLeft,0);
		//接收到RTSP数据
		if (m_nRecvLen > 0)
		{
			//有效数据长度
			int32 nNewByteRead =0;
			//如果接收到数据 更新当前时间
			ftime(&m_lasttb);
			//去掉空字符
			//for(int i=0;i<m_nRecvLen;i++)
			//{
				/*if(m_pResponseBuffer[m_nResponseBytesAlreadySeen + i] == '\0')
					continue;*/
			//	m_pResponseBuffer[m_nResponseBytesAlreadySeen + nNewByteRead] = m_pResponseBuffer[m_nResponseBytesAlreadySeen + i];
				//nNewByteRead++;
			//}
			if (m_nVideoFlag == 4)
			{
				//TRACE("is in this\r\n");
				printf("is in this\r\n");
			}

			//添加结束符
			//m_pResponseBuffer[m_nResponseBytesAlreadySeen + nNewByteRead] = '\0';
			//yxj,加入解析tcp包中的rtcp部分，过滤rtcp
			nNewByteRead = this->ParseTCPData();
			//添加结束符
			m_pResponseBuffer[m_nResponseBytesAlreadySeen + nNewByteRead] = '\0';
			//解析RTSP回应数据
			handleResponseBytes(nNewByteRead);
		}
	}
	else
	{
		//在TCP端口上接收RTP数据
		m_nRecvLen = ReadRTPoverTCP();
		//接收到数据 更新最后通讯时间
		if (m_nRecvLen > 0)
		{
			//处理RTP数据
			if(m_fStreamChannelId == m_nRtpChannelId)
				DealRTPData((BYTE*)m_chRecvBuf, (unsigned short)m_nRTPLen);
			//处理RTCP数据
			if(m_fStreamChannelId == m_nRtcpChannelId)
			{
				//验证是否rtcp数据包
				if(Validate_rtcp(m_chRecvBuf, m_nRTPLen))
				{
					//获取接收发送者报告的时间
					gettimeofday(&m_lastRTCP, NULL);
				}
			}
		}
		//发送接收者报告
		SendRecvReport();
	}
	//得到当前时间
    ftime(&m_curtb);
	//判断是否到达心跳包监测时间,采用TCP发送 字符“maxfort+#接收到的数据长度
	subTime = (int32)((m_curtb.time - m_lasttb.time)*1000 + (m_curtb.millitm-m_lasttb.millitm));
	if(subTime > CHECKTIME)
	{
		////如果长时间接收不到数据 应该关闭连接
		if(m_nTickCount > 2)
		{
			m_nRecvLen = (-1);
		}
		//修改计数为了处理 如果接收不到UDP数据时也重连
		m_nTickCount++;
#ifdef _DEBUG
#ifdef RTSPCLIENT_DEBUG_LEVEL_1
		//调试信息打印帧速
		TRACE("cam:%s %d毫秒-%d帧, 丢包数:%d\n",m_chVideoName, subTime,m_nFramePerSec, m_pH264RTPunPack->m_loastPack);
#endif
#endif
		//清空帧计数
		m_nFramePerSec = 0;
		//得到当前时间
		m_lasttb = m_curtb;
	}
	//连接故障需要断开连接
	if(m_nRecvLen < 0)
	{
		//TRACE("连接故障，断开重连！\r\n");
		printf("连接故障，断开重连！\r\n");
		//发送断开连接命令
		sendTEARDOWNCmd();
		//关闭套接字
		TCP_Close(m_nRtspSocket);
		m_nRtspSocket = -1;
		//RTP包序号
		m_nRtpSeq = 0;
		//2016,12,20,清除cseq,清除descrive中的nonce和realm
		m_nWaitCSeq = 0;
		m_pOurAuthenticator->setRealmAndNonce(NULL,NULL);
		//当前状态
		m_nVideoFlag = 0;
		//更新当前时间
		ftime(&m_lasttb);
		return 1;
	}
	return 0;
}

//解析rtsp的tcp包，不是有效数据(rtcp or rtp)则丢掉
int CRtspClient::ParseTCPData()
{
	int i = 0;
	int32 nNewByteRead =0;
	if (this->seekFlag == 1)
	{
		//如果偏移长度大于数据长度
		if (this->packLen >= m_nRecvLen)
		{
			// FIXME:这是要干嘛？
			seekFlag - m_nRecvLen;

			return 0;
		}
		//否则偏移
		else
		{
			i += this->packLen;
		}
	}
	for(;i<m_nRecvLen;i++)
	{
		//如果不是rtsp回应，（RTCP或RTP）
		if (m_pResponseBuffer[m_nResponseBytesAlreadySeen+i] == 36)
		{
			//如果长度超过接受长度，退出
			/*if ((i+3) >= m_nRecvLen)
			{
				break;
			}*/
			if (m_pResponseBuffer[m_nResponseBytesAlreadySeen+i+1] == 2)
			{
				;
			}
			if (m_pResponseBuffer[m_nResponseBytesAlreadySeen+i+1] == 3)
			{
				;
			}
			unsigned int highbyte = m_pResponseBuffer[m_nResponseBytesAlreadySeen+i+2];
			highbyte = highbyte << 8;
			packLen = highbyte + (unsigned char)(m_pResponseBuffer[m_nResponseBytesAlreadySeen+i+3]);
			if (packLen > m_nRecvLen)
			{
				// FIXME：这是要干嘛？
				packLen - m_nRecvLen + 4;

				this->seekFlag = 1;

				return 0;
			}
			i += packLen + 3;
			//如果偏移长度超过接收长度，退出
			/*if (i >= m_nRecvLen)
			{
				break;
			}*/
			continue;
		}
		m_pResponseBuffer[m_nResponseBytesAlreadySeen + nNewByteRead] = m_pResponseBuffer[m_nResponseBytesAlreadySeen + i];
		nNewByteRead++;
	}
	return nNewByteRead;
}
/*************************************************/
//RTCP处理相关函数
//验证是否RTCP数据包
BOOL CRtspClient::Validate_rtcp(int8 *packet, int len)
{
	rtcp_t	*pkt  = (rtcp_t *) packet;
	rtcp_t	*end  = (rtcp_t *) (((char *) pkt) + len);
	rtcp_t	*r    = pkt;
	int	 l    = 0;
	int	 pc   = 1;
	int	 p    = 0;
	if (((ntohs(pkt->common.length) + 1) * 4) == len)
	{
		return FALSE;
	}
	if (pkt->common.version != 2)
	{
		return FALSE;
	}
	if (pkt->common.p != 0)
	{
		return FALSE;
	}
	if (pkt->common.pt != RTCP_SR)
	{
		return FALSE;
	}
	do
	{
		if (p == 1)
		{
			return FALSE;
		}
		if (r->common.p)
		{
			p = 1;
		}
		if (r->common.version != 2)
		{
			return FALSE;
		}
		l += (ntohs(r->common.length) + 1) * 4;
		r  = (rtcp_t *) (((uint32 *) r) + ntohs(r->common.length) + 1);
		pc++;
	}while (r < end);
	if (l != len)
	{
		return FALSE;
	}
	if (r != end)
	{
		return FALSE;
	}

	//SR的ntp时间中间32位
	m_nNtpTime= (pkt->r.sr.sr.ntp_frac<<16)|(pkt->r.sr.sr.ntp_sec>>16);
	m_nSsrc = pkt->r.sr.sr.ssrc;

	return TRUE;
}

//组织RR回应数据包
uint32 CRtspClient::RTCPRRRespose(char* buffer,int length)
{
	struct timeval timeNow, timeSinceLSR;
	unsigned int DelayLSR = 0;
	uint8 pos = 0;//偏移量
	uint8 len = 0;
	//判断输出缓冲区长度
	if(length < 128)
	{
		return 0;
	}
	//TCP方式传输 RTCP回应包 组织头信息
	if(m_bOverTCP)
	{
		buffer[pos++]  = (BYTE)'$';							// 标示符
		buffer[pos++]  = (BYTE)3;							// 通道ID
		buffer[pos++]  = (BYTE)0;							// 数据长度高字节
		buffer[pos++]  = (BYTE)0;							// 数据长度低字节
	}
	//组织RR接收者报文
	buffer[pos++]  = (BYTE)0x81;							// V=2,  P=0,RC=1
	buffer[pos++]  = (BYTE)RTCP_RR;							// PT=RR=201
	buffer[pos++]  = (BYTE)0x00;							//
	buffer[pos++]  = (BYTE)0x07;							// length (7 32-bit words, minus one)
	*(unsigned int*)&buffer[pos] = (unsigned int)m_nCsrc;	pos+=4;	//
	*(unsigned int*)&buffer[pos] = (unsigned int)m_nSsrc;	pos+=4;	//
	buffer[pos++]  = (BYTE)0x00;									// 丢包率
	buffer[pos++]  = (BYTE)0x00;
	buffer[pos++]  = (BYTE)0x00;
	buffer[pos++]  = (BYTE)0x00;
	*(unsigned int*)&buffer[pos] = (unsigned int)m_nRtpSeq;	pos+=4;	// RTP包序号
	*(unsigned int*)&buffer[pos] = (unsigned int)0;			pos+=4;	// 时间抖动
	*(unsigned int*)&buffer[pos] = (unsigned int)m_nNtpTime;pos+=4;	// 接收到的来自源SSRC_n的最新RTCP发送者报告(SR)的64位NTP时间标志的中间32位

	//获取当前时间
	gettimeofday(&timeNow, NULL);
	if (timeNow.tv_usec < m_lastRTCP.tv_usec)
	{
		timeNow.tv_usec += 1000000;
		timeNow.tv_sec -= 1;
	}
	//计算从收到来自SSRC_n的SR包到发送此接收报告块之间的延时
	timeSinceLSR.tv_sec = timeNow.tv_sec - m_lastRTCP.tv_sec;
	timeSinceLSR.tv_usec = timeNow.tv_usec - m_lastRTCP.tv_usec;
	DelayLSR = (timeSinceLSR.tv_sec<<16) | ( (((timeSinceLSR.tv_usec<<11)+15625)/31250) & 0xFFFF);
	// 自上一SR的时间(DLSR)：32比特   是从收到来自SSRC_n的SR包到发送此接收报告块之间的延时，以1/65536秒为单位。若还未收到来自SSRC_n的SR包，该域值为零
	if(m_nNtpTime != 0)
	{
		*(unsigned int*)&buffer[pos] = (unsigned int)ntohl(DelayLSR);
	}
	else
	{
		*(unsigned int*)&buffer[pos] = (unsigned int)0;
	}
	pos+=4;

	//计算长度
	len = (4 + 2 + (g_nHostLen + 1) + 3)/4;
	//组织SDES描述信息报文
	buffer[pos++]  = (BYTE)0x81;							// V=2,  P=0,RC=1
	buffer[pos++]  = (BYTE)RTCP_SDES;						// PT=SDES=202
	buffer[pos++]  = (BYTE)0x00;							//
	buffer[pos++]  = (BYTE)len;								// length 32bit words
	*(unsigned int*)&buffer[pos]=(unsigned int)m_nCsrc;	pos+=4;
	buffer[pos++]  = (BYTE)0x01;							//项数
	buffer[pos++]  = (BYTE)g_nHostLen;						//项长度
	memcpy(&buffer[pos],g_chHostName,g_nHostLen);			//拷贝项数据
	pos += g_nHostLen;
	//从双字长度转换为字节长度
	len = ((pos+3)/4)*4;
	//补齐32位对齐数据
	while(pos < len)
	{
		buffer[pos++] = (BYTE)0x00;
	}
	//TCP方式传输 RTCP回应包 给信息头中的长度赋值
	if(m_bOverTCP)
	{
		buffer[2]  = (BYTE)(0x00);							// 数据长度高字节
		buffer[3]  = (BYTE)(len-4);							// 数据长度低字节
		return (len);
	}
	//返回封装RTCP包的总长度
	return len;
}
//发送接收者报告
int8 CRtspClient::SendRecvReport()
{
	//获取当前时间
	//FILETIME curTime;
	struct timeval curTime;

	//时间差 - 单位： 100纳秒
	DWORD	 sunTime;

	//获取当前时间
	//::GetSystemTimeAsFileTime(&curTime);
	gettimeofday(&curTime,NULL);

	// FIXME: 检查此处代码正确性。
	//计算时间差
//	if(curTime.dwLowDateTime < m_sLastTime.dwLowDateTime)
//		sunTime = (0xFFFFFFFF - m_sLastTime.dwLowDateTime) + curTime.dwLowDateTime;
//	else
//		sunTime = curTime.dwLowDateTime - m_sLastTime.dwLowDateTime;
	sunTime = ((curTime.tv_sec - m_sLastTime.tv_sec) * 1000000 + curTime.tv_usec - m_sLastTime.tv_usec) / 10;

	//判断是否需要发送RTCP数据报告，转换成100那秒单位时间进行比较
	if(sunTime < m_nReportInterval*10000)
	{
		return 1;
	}
	//判断发送rtcp命令的套接字是否关闭
	if(m_bOverTCP)
	{
		if(m_nRtspSocket <= 0)
			return 0;
	}
	else  if(m_nRtcpSocket <= 0)
	{
		return 0;
	}
	//发送数据长度 用于判断是否成功
	int   sendLen;
	//更新时间
	m_sLastTime = curTime;
	//分配内存
	char *rrBuf = new char[256];
	//组织接收者报告
	int rrLen = RTCPRRRespose(rrBuf,256);
	//发送RTCP回应
	if(m_bOverTCP)
	{
		sendLen = TCP_Send(m_nRtspSocket, rrBuf, rrLen);
	} else
	{
		sendLen = UDP_Send(m_nRtcpSocket,m_nServerIP, m_nServerRTCPPort ,rrBuf,rrLen);
	}
	//释放内存
	free(rrBuf);rrBuf=NULL;
	//发送失败
	if(sendLen != rrLen)
	{
		return 0;
	}
	//发送成功
	return 1;
}

//发送离开报告
int8 CRtspClient::SendBYE()
{
	struct timeval timeNow, timeSinceLSR;
	unsigned int DelayLSR = 0;
	BYTE* byeBuf = NULL;
	uint8 byeLen	 = 0;	//偏移量
	int   sendLen;

	//判断发送rtcp命令的套接字是否关闭
	if(m_bOverTCP)
	{
		if(m_nRtspSocket <= 0)
			return 0;
	}
	else  if(m_nRtcpSocket <= 0)
	{
		return 0;
	}
	//分配内存
	byeBuf = new BYTE[256];
	//内存分配失败 直接返回
	if(byeBuf == NULL)
	{
		return 0;
	}
	//TCP方式传输 RTCP回应包 组织头信息
	if(m_bOverTCP)
	{
		byeBuf[byeLen++]  = (BYTE)'$';						// 标示符
		byeBuf[byeLen++]  = (BYTE)3;						// 通道ID
		byeBuf[byeLen++]  = (BYTE)0;						// 数据长度高字节
		byeBuf[byeLen++]  = (BYTE)0;						// 数据长度低字节
	}
	//组织RR接收者报文
	byeBuf[byeLen++]  = (BYTE)0x81;							// V=2,  P=0,RC=1
	byeBuf[byeLen++]  = (BYTE)RTCP_RR;						// PT=RR=201
	byeBuf[byeLen++]  = (BYTE)0x00;							//
	byeBuf[byeLen++]  = (BYTE)0x07;							// length (7 32-bit words, minus one)
	*(unsigned int*)&byeBuf[byeLen]=(unsigned int)m_nCsrc;	byeLen+=4;	//
	*(unsigned int*)&byeBuf[byeLen]= (unsigned int)m_nSsrc;	byeLen+=4;	//
	*(unsigned int*)&byeBuf[byeLen]= (unsigned int)0;		byeLen+=4;	// 丢包率
	*(unsigned int*)&byeBuf[byeLen]=(unsigned int)m_nRtpSeq;byeLen+=4;	// RTP包序号
	*(unsigned int*)&byeBuf[byeLen]= (unsigned int)0;		byeLen+=4;	// 时间抖动
	*(unsigned int*)&byeBuf[byeLen]= (unsigned int)m_nNtpTime;byeLen+=4;// 接收到的来自源SSRC_n的最新RTCP发送者报告(SR)的64位NTP时间标志的中间32位

	//获取当前时间
	gettimeofday(&timeNow, NULL);
	if (timeNow.tv_usec < m_lastRTCP.tv_usec)
	{
		timeNow.tv_usec += 1000000;
		timeNow.tv_sec -= 1;
	}
	//计算从收到来自SSRC_n的SR包到发送此接收报告块之间的延时
	timeSinceLSR.tv_sec = timeNow.tv_sec - m_lastRTCP.tv_sec;
	timeSinceLSR.tv_usec = timeNow.tv_usec - m_lastRTCP.tv_usec;
	DelayLSR = (timeSinceLSR.tv_sec<<16) | ( (((timeSinceLSR.tv_usec<<11)+15625)/31250) & 0xFFFF);
	// 自上一SR的时间(DLSR)：32比特   是从收到来自SSRC_n的SR包到发送此接收报告块之间的延时，以1/65536秒为单位。若还未收到来自SSRC_n的SR包，该域值为零
	*(unsigned int*)&byeBuf[byeLen] = (unsigned int)ntohl(DelayLSR);byeLen+=4;

	//添加BYE
	byeBuf[byeLen++]  = (BYTE)0x81;							// V=2,  P=0,RC=1
	byeBuf[byeLen++]  = (BYTE)RTCP_BYE;						// PT=BYE=203
	byeBuf[byeLen++]  = (BYTE)0x00;							//
	byeBuf[byeLen++]  = (BYTE)0x01;							// length (1 32-bit words, minus one)
	*(unsigned int*)&byeBuf[byeLen]=(unsigned int)m_nSsrc;	byeLen+=4;

	//TCP方式传输 RTCP回应包 给信息头中的长度赋值
	if(m_bOverTCP)
	{
		byeBuf[2]  = (BYTE)(0x00);							// 数据长度高字节
		byeBuf[3]  = (BYTE)(byeLen-4);						// 数据长度低字节
		//发送bye数据包
		sendLen = TCP_Send(m_nRtspSocket, (const char*)byeBuf, byeLen);
	}
	else
	{
		//发送bye数据包
		sendLen = UDP_Send(m_nRtcpSocket, m_nServerIP, m_nServerRTCPPort,(const char*)byeBuf, byeLen);
	}
	//释放内存
	free(byeBuf);byeBuf=NULL;
	//发送失败
	if(sendLen != byeLen)
	{
		return 0;
	}
	//发送成功
	return 1;
}

/**************************************************/
//接收处理UDP数据
int8 CRtspClient::DealUDPData()
{
	//当前状态还没有开始播放
	if(this->m_nVideoFlag <= 4)
	{
		//延时 直接返回
		//Sleep(1);
		usleep(1*1000);

		return 1;
	}
	//TCP模式 不处理UDP数据
	if(m_bOverTCP)
	{
		return 1;
	}
	//判断接收RTCP数据的套接字是否打开
	if(m_nRtcpSocket > 0)
	{
		//接收RTCP数据
		m_nRTCPLen = UDP_Recv(m_nRtcpSocket, m_nFromIP, m_nFromPort,m_chRTCPBuf,256,0);
		if(m_nRTCPLen > 0)
		{
			//判断IP和端口信息是否一致
			if((m_nFromIP == m_nServerIP) && (m_nFromPort == m_nServerRTCPPort))
			{
				//验证是否rtcp数据包
				if(Validate_rtcp(m_chRTCPBuf, m_nRTCPLen))
				{
					//获取接收发送者报告的时间
					gettimeofday(&m_lastRTCP, NULL);
				}
			}
		}
		//发送接收者报告
		SendRecvReport();
	}
	//调用CommUDP模块的UDP_Recv函数接收视频数据
	if(m_nRtpSocket > 0)
	{
		//接收RTP数据包
		m_nRecvLen = UDP_Recv(m_nRtpSocket, m_nFromIP, m_nFromPort, m_chRecvBuf, RTP_BUFFER_SIZE, 1);
		//未接收到RTP包
		if(m_nRecvLen <= 0)
		{
			return 1;
		}
		//处理RTP数据
		DealRTPData((BYTE*)m_chRecvBuf, (unsigned short)m_nRecvLen);
	}
	else
	{
		//Sleep(10);
		usleep(10*1000);
	}
	return 0;
}

//处理RTP数据
int8 CRtspClient::DealRTPData(BYTE* chData, unsigned short nLen)
{
	//接收到数据时,计数要清零
	m_nTickCount = 0;
	/*************************************/
	//累计统计结果 发送包数 发送总长度
	m_nToalSuccPackNum++;
	m_nTotalSuccLen += nLen;
	//节点类型为2时才处理转发
	if(m_nAppType == 2)
	{
#ifdef RTP_THREAD_FLAG
		//转发队列已满
		if((m_nRtpQueueTail + 1)%RTP_QUEUESIZE == m_nRtpQueueHead)
		{
			return 0;
		}
		//如果数据长度大于分配的长度 先释放原来分配的空间
		if((m_pRtpQueue[m_nRtpQueueTail].pData != NULL) && (m_pRtpQueue[m_nRtpQueueTail].nSize < nLen))
		{
			delete[] m_pRtpQueue[m_nRtpQueueTail].pData; m_pRtpQueue[m_nRtpQueueTail].pData = NULL;
		}
		//拷贝RTP包到队列 由发送线程处理转发到各个客户端节点
		if(m_pRtpQueue[m_nRtpQueueTail].pData == NULL)
		{
			m_pRtpQueue[m_nRtpQueueTail].pData = new uint8[nLen];
			m_pRtpQueue[m_nRtpQueueTail].nSize = nLen;
		}
		//将数据保存到队列
		if(m_pRtpQueue[m_nRtpQueueTail].pData != NULL)
		{
			memcpy(m_pRtpQueue[m_nRtpQueueTail].pData, chData, nLen);
			m_pRtpQueue[m_nRtpQueueTail].nLen = nLen;
			//移动队列尾指针
			m_nRtpQueueTail = (m_nRtpQueueTail + 1)%RTP_QUEUESIZE;
		}
#else
		//互斥访问
		AcquireSRWLockExclusive(&m_pTranSRWLock);
		//数据转发
		for(STranNodeInfo* pNodeInfo=m_pTranNodeInfo; pNodeInfo != NULL;)
		{
			//将数据转发到其它节点
			if(pNodeInfo->nNodePort != 0)
			{
				UDP_Send(pNodeInfo->nSocket, pNodeInfo->nNodeIP, pNodeInfo->nNodePort ,(const char*)chData,nLen);
			}
			else if(pNodeInfo->nSocket)
			{
				uint8 temp[4];
				//发送标示信息
				temp[0] = (uint8)'$';
				temp[1] = (uint8)(pNodeInfo->nChannelId);
				temp[2] = (uint8)((nLen>>8)&0xFF);
				temp[3] = (uint8)(nLen&0xFF);
				//发送RTP数据
				TCP_Send(pNodeInfo->nSocket, (const char*)temp,4);
				//发送RTP数据
				TCP_Send(pNodeInfo->nSocket, (const char*)chData,nLen);
			}
			//获取下一个节点
			pNodeInfo = (STranNodeInfo*)(pNodeInfo->pNextObj);
		}
		//释放互斥同步对象
		ReleaseSRWLockExclusive(&m_pTranSRWLock);
#endif
	}
	//数据队列为空
	if(m_pVideoQueue != NULL)
	{
		//数据结构
		if(m_stDataSrc == NULL)
		{
			//获取队列尾元素指针
			if(m_pVideoQueue->GetFreeSizeEx(&m_stDataSrc) == 0)
			{
				return 0;
			}
			//判断结构参数是否合法
			if((m_stDataSrc == NULL) || (m_stDataSrc->pVoidData == NULL))
			{
				m_stDataSrc = NULL;
				return 0;
			}
			//设置解包缓冲区
			m_pH264RTPunPack->SetBuffer((BYTE*)(m_stDataSrc->pVoidData), m_stDataSrc->dataLen);
		}
	}
	else if(m_pVideoQueueEx != NULL)
	{
		//数据结构
		if(m_stDataSrc == NULL)
		{
			//获取队列尾元素指针
			if(m_pVideoQueueEx->queGetFreeSizeEx((StMyQueueData**)&m_stDataSrc) <= 0)
			{
				return 0;
			}
			//判断结构参数是否合法
			if((m_stDataSrc == NULL) || (m_stDataSrc->pVoidData == NULL))
			{
				m_stDataSrc = NULL;
				return 0;
			}
			//设置解包缓冲区
			m_pH264RTPunPack->SetBuffer((BYTE*)(m_stDataSrc->pVoidData), m_stDataSrc->dataLen);
		}
	}
	else
	{
		return 0;
	}
	//没有完整包
	if(m_pH264RTPunPack->Parse_RTP_Packet((BYTE*)chData, nLen, &m_nFrameSize, &m_nFrametype,(unsigned int*)&m_nRtpSeq,(unsigned int*)&m_nSsrc, &m_nFrameTs, &m_nLostFlag) == NULL)
	{
		return 1;
	}
	//统计帧数
	m_nFramePerSec++;
	//帧序号
	m_nOutPackNum++;
	if(m_nLostFlag == 1)
	{
		m_nOutPackNum++;
	}
	//兼容旧版本视频队列
	if(m_pVideoQueue != NULL)
	{
		//获取队列剩余空间
		m_nFreeSize = m_pVideoQueue->GetFreeSize();
		//判断队列剩余空间
		if(m_nFreeSize <= 2)
		{
			//队列满 丢弃当前帧
			if(m_nFreeSize == 0)
			{
				return 0;
			}
			//如果数据帧不是I帧数据,队列空闲小于等于2时就丢弃，为了预留存放I帧的空间
			if(m_nFrametype != 0x01)
			{
				return 0;
			}
		}
		//组织数据并将数据放入视频队列
		//添加协议头
		memcpy((BYTE*)m_stDataSrc->pVoidData,"DATA",4);
		memcpy((BYTE*)m_stDataSrc->pVoidData + 4,&m_nOutPackNum,4);
		//添加帧类型 帧类型为：I帧
		if(m_nFrametype == 0x01)
		{
			*((BYTE*)(m_stDataSrc->pVoidData) + 8) = 0x01;
		}
		else
		{
			/*其他帧类型 P帧或B帧*/
			*((BYTE*)(m_stDataSrc->pVoidData) + 8) = 0x02;
		}
		//更新数据长度
		m_stDataSrc->dataLen = m_nFrameSize;	//视频信息长度
		m_stDataSrc->lengthOFExtaraInfo = 4;	//附加信息长度
		memcpy(m_stDataSrc->pVoidExtraInfo, &m_nFrameTs,4);
		//将视频数据放入队列
		if(!(m_pVideoQueue->quePutEx(&m_stDataSrc)))
		{
#ifdef _DEBUG
#ifdef RTSPCLIENT_DEBUG_LEVEL_2
			TRACE("Client 放入队列失败\n");
#endif
#endif
		}
	}
	//新版本队列
	else if(m_pVideoQueueEx != NULL)
	{
		//组织数据并将数据放入视频队列
		//添加协议头
		memcpy((BYTE*)m_stDataSrc->pVoidData,"DATA",4);
		memcpy((BYTE*)m_stDataSrc->pVoidData + 4,&m_nOutPackNum,4);
		//添加帧类型 帧类型为：I帧
		if(m_nFrametype == 0x01)
		{
			*((BYTE*)(m_stDataSrc->pVoidData) + 8) = 0x01;
		}
		else
		{
			/*其他帧类型 P帧或B帧*/
			*((BYTE*)(m_stDataSrc->pVoidData) + 8) = 0x02;
		}
		//更新数据长度
		m_stDataSrc->dataLen = m_nFrameSize;	//视频信息长度
		m_stDataSrc->lengthOFExtaraInfo = 4;	//附加信息长度
		//附加信息
		m_stDataSrc->pVoidExtraInfo = ((char*)(m_stDataSrc->pVoidData) + m_nFrameSize);
		memcpy(m_stDataSrc->pVoidExtraInfo, &m_nFrameTs,4);
		//将视频数据放入队列
		if(!(m_pVideoQueueEx->quePut((StMyQueueData**)&m_stDataSrc)))
		{
#ifdef _DEBUG
#ifdef RTSPCLIENT_DEBUG_LEVEL_2
			TRACE("Client 放入队列失败\n");
#endif
#endif
		}
	}

	//判断结构参数是否合法
	if((m_stDataSrc == NULL) || (m_stDataSrc->pVoidData == NULL))
	{
		m_stDataSrc = NULL;
		return 1;
	}
	//设置解包缓冲区
	m_pH264RTPunPack->SetBuffer((BYTE*)(m_stDataSrc->pVoidData), m_stDataSrc->dataLen);
	return 1;
}

#ifdef RTP_THREAD_FLAG
//转发RTP数据
void CRtspClient::TranDataFunc()
{
	//没有数据需要转发
	if(m_nRtpQueueHead == m_nRtpQueueTail)
	{
		//Sleep(1);
		usleep(1*1000);

		return;
	}

	//互斥访问
	//AcquireSRWLockExclusive(&m_pTranSRWLock);
	pthread_mutex_lock(&m_pTranSRWLock);

	//数据转发
	for(STranNodeInfo* pNodeInfo=m_pTranNodeInfo; pNodeInfo != NULL;)
	{
		//将数据转发到其它节点
		if(pNodeInfo->nNodePort != 0)
		{
			UDP_Send(pNodeInfo->nSocket, pNodeInfo->nNodeIP, pNodeInfo->nNodePort, (const char*)(m_pRtpQueue[m_nRtpQueueHead].pData), m_pRtpQueue[m_nRtpQueueHead].nLen);
		}
		else if((pNodeInfo->nSocket>0) && (pNodeInfo->bActive==1))
		{
			uint8 temp[4];
			//发送标示信息
			temp[0] = (uint8)'$';
			temp[1] = (uint8)(pNodeInfo->nChannelId);
			temp[2] = (uint8)((m_pRtpQueue[m_nRtpQueueHead].nLen>>8)&0xFF);
			temp[3] = (uint8)(m_pRtpQueue[m_nRtpQueueHead].nLen&0xFF);
			//发送RTP数据
			TCP_Send(pNodeInfo->nSocket, (const char*)temp,4);
			//发送RTP数据
			if(TCP_Send(pNodeInfo->nSocket, (const char*)(m_pRtpQueue[m_nRtpQueueHead].pData), m_pRtpQueue[m_nRtpQueueHead].nLen) <= 0)
				pNodeInfo->bActive = 0;
		}
		//获取下一个节点
		pNodeInfo = (STranNodeInfo*)(pNodeInfo->pNextObj);
	}
	//移动头指针
	m_nRtpQueueHead = (m_nRtpQueueHead + 1)%RTP_QUEUESIZE;
	//释放互斥同步对象
	//ReleaseSRWLockExclusive(&m_pTranSRWLock);
	pthread_mutex_unlock(&m_pTranSRWLock);
}
#endif
/*****************************************************************
函数功能：	得到数据统计信息
函数接口：	int8 GetDataCheck(double* nRecvPack, double* nRecvLen)
入口参数：	无；
出口参数：	nRecvPack	接收到的包数；
			nRecvLen	接收到的数据长度
返 回 值：	0：成功，1：失败
函数功能：	停止视频数据下载，断开连接
******************************************************************/
int8 CRtspClient::GetDataCheck(double* nRecvPack, double* nRecvLen)
{
	//接收的包数
	*nRecvPack = m_nToalSuccPackNum;
	//接收到的长度
	*nRecvLen  = m_nTotalSuccLen;
	return 0;
}

//添加节点信息 UDP通讯方式
int32   CRtspClient::AddSubNode(const int32 nSocket, const uint32 nNodeIP, const uint16 nNodePort)
{
	//互斥访问
	//AcquireSRWLockExclusive(&m_pTranSRWLock);
	pthread_mutex_lock(&m_pTranSRWLock);

	//如果节点链表不存在直接创建
	if(m_pTranNodeInfo == NULL)
	{
		//创建新节点
		m_pTranNodeInfo = new STranNodeInfo;
		//保存节点信息
		m_pTranNodeInfo->nNodeIP	= nNodeIP;
		m_pTranNodeInfo->nNodePort	= nNodePort;
		m_pTranNodeInfo->nSocket	= nSocket;
		m_pTranNodeInfo->nChannelId	= 0;
		m_pTranNodeInfo->bActive	= 1;
		m_pTranNodeInfo->pNextObj	= NULL;
		m_pTranNodeInfo->pPreObj	= NULL;
		//释放互斥同步对象
		//ReleaseSRWLockExclusive(&m_pTranSRWLock);
		pthread_mutex_unlock(&m_pTranSRWLock);
		return 0;
	}
	//节点链表存在 从链表中查找是否有重复节点；如果有重复节点 则不用添加；如果没有重复节点，则创建新节点加入到链表
	for(STranNodeInfo* pNodeInfo=m_pTranNodeInfo; pNodeInfo != NULL; pNodeInfo = (STranNodeInfo*)(pNodeInfo->pNextObj))
	{
		//判断节点是否已存在 如果存在直接返回成功
		if((pNodeInfo->nNodeIP == nNodeIP) && (pNodeInfo->nNodePort ==  nNodePort))
		{
			break;
		}
		//如果没有找到相同的节点则添加
		if(pNodeInfo->pNextObj == NULL)
		{
			//创建新节点
			STranNodeInfo* pNewNodeInfo = new STranNodeInfo;
			//保存节点信息
			pNewNodeInfo->nNodeIP	= nNodeIP;
			pNewNodeInfo->nNodePort	= nNodePort;
			pNewNodeInfo->nSocket	= nSocket;
			pNewNodeInfo->nChannelId	= 0;
			pNewNodeInfo->bActive	= 1;
			pNewNodeInfo->pNextObj	= NULL;
			//将节点插入到链表
			pNodeInfo->pNextObj		= pNewNodeInfo;
			pNewNodeInfo->pPreObj	= pNodeInfo;
			break;
		}
	}
	//释放互斥同步对象
	//ReleaseSRWLockExclusive(&m_pTranSRWLock);
	pthread_mutex_unlock(&m_pTranSRWLock);
	return 1;
}

//添加节点信息 TCP通讯方式
int32   CRtspClient::AddSubNode(const int32 nSocket, const uint8 nChannelId)
{
	//互斥访问
	//AcquireSRWLockExclusive(&m_pTranSRWLock);
	pthread_mutex_lock(&m_pTranSRWLock);

	//如果节点链表不存在直接创建
	if(m_pTranNodeInfo == NULL)
	{
		//创建新节点
		m_pTranNodeInfo = new STranNodeInfo;
		//保存节点信息
		m_pTranNodeInfo->nNodeIP	= 0;
		m_pTranNodeInfo->nNodePort	= 0;
		m_pTranNodeInfo->nSocket	= nSocket;
		m_pTranNodeInfo->nChannelId	= nChannelId;
		m_pTranNodeInfo->bActive	= 1;
		m_pTranNodeInfo->pNextObj	= NULL;
		m_pTranNodeInfo->pPreObj	= NULL;
		//释放互斥同步对象
		//ReleaseSRWLockExclusive(&m_pTranSRWLock);
		pthread_mutex_unlock(&m_pTranSRWLock);
		return 0;
	}
	//节点链表存在 从链表中查找是否有重复节点；如果有重复节点 则不用添加；如果没有重复节点，则创建新节点加入到链表
	for(STranNodeInfo* pNodeInfo=m_pTranNodeInfo; pNodeInfo != NULL; pNodeInfo = (STranNodeInfo*)(pNodeInfo->pNextObj))
	{
		//判断节点是否已存在 如果存在直接返回成功
		if((pNodeInfo->nSocket ==  nSocket) && (pNodeInfo->nChannelId ==  nChannelId))
			break;
		//如果没有找到相同的节点则添加
		if(pNodeInfo->pNextObj == NULL)
		{
			//创建新节点
			STranNodeInfo* pNewNodeInfo = new STranNodeInfo;
			//保存节点信息
			pNewNodeInfo->nNodeIP	= 0;
			pNewNodeInfo->nNodePort	= 0;
			pNewNodeInfo->nSocket	= nSocket;
			pNewNodeInfo->nChannelId	= nChannelId;
			pNewNodeInfo->bActive	= 1;
			//将节点插入到链表
			pNewNodeInfo->pNextObj	= NULL;
			pNewNodeInfo->pPreObj	= pNodeInfo;
			pNodeInfo->pNextObj		= pNewNodeInfo;
			break;
		}
	}
	//释放互斥同步对象
	//ReleaseSRWLockExclusive(&m_pTranSRWLock);
	pthread_mutex_unlock(&m_pTranSRWLock);
	return 1;
}
//删除节点信息 UDP通讯方式
int32   CRtspClient::RemoveSubNode(const int32 nSocket, const uint32 nNodeIP, const uint16 nNodePort)
{
	//如果节点链表不存在直接返回
	if(m_pTranNodeInfo == NULL)
	{
		return 0;
	}
	//互斥访问
	//AcquireSRWLockExclusive(&m_pTranSRWLock);
	pthread_mutex_lock(&m_pTranSRWLock);

	//节点链表存在 从链表中查找节点是否存在；如果节点存在 则删除；如果节点不存在，不用处理
	for(STranNodeInfo* pNodeInfo=m_pTranNodeInfo; pNodeInfo != NULL; pNodeInfo = (STranNodeInfo*)(pNodeInfo->pNextObj))
	{
		//匹配要查找的节点信息
		if((pNodeInfo->nNodeIP != nNodeIP) || (pNodeInfo->nNodePort !=  nNodePort) || (pNodeInfo->nSocket != nSocket))
		{
			continue;
		}
		//从链表中删除当前节点信息
		STranNodeInfo* pPreNodeInfo = (STranNodeInfo*)(pNodeInfo->pPreObj);
		STranNodeInfo* pNextNodeInfo = (STranNodeInfo*)(pNodeInfo->pNextObj);
		if(pPreNodeInfo != NULL)
		{
			pPreNodeInfo->pNextObj = pNextNodeInfo;
		}
		else
		{
			m_pTranNodeInfo = pNextNodeInfo;
		}
		if(pNextNodeInfo != NULL)
		{
			pNextNodeInfo->pPreObj = pPreNodeInfo;
		}
		//释放当前节点
		delete[] pNodeInfo;
		break;
	}
	//释放互斥同步对象
	//ReleaseSRWLockExclusive(&m_pTranSRWLock);
	pthread_mutex_unlock(&m_pTranSRWLock);

	return 0;
}
//删除节点信息 TCP通讯方式
int32   CRtspClient::RemoveSubNode(const int32 nSocket, const uint8 nChannelId)
{
	//如果节点链表不存在直接返回
	if(m_pTranNodeInfo == NULL)
	{
		return 0;
	}
	//互斥访问
	//AcquireSRWLockExclusive(&m_pTranSRWLock);
	pthread_mutex_lock(&m_pTranSRWLock);

	//节点链表存在 从链表中查找节点是否存在；如果节点存在 则删除；如果节点不存在，不用处理
	for(STranNodeInfo* pNodeInfo=m_pTranNodeInfo; pNodeInfo != NULL; pNodeInfo = (STranNodeInfo*)(pNodeInfo->pNextObj))
	{
		//匹配要查找的节点信息
		if((pNodeInfo->nSocket !=  nSocket) || (pNodeInfo->nChannelId !=  nChannelId))
		{
			continue;
		}
		//从链表中删除当前节点信息
		STranNodeInfo* pPreNodeInfo = (STranNodeInfo*)(pNodeInfo->pPreObj);
		STranNodeInfo* pNextNodeInfo = (STranNodeInfo*)(pNodeInfo->pNextObj);
		if(pPreNodeInfo != NULL){
			pPreNodeInfo->pNextObj = pNextNodeInfo;
		}
		else
		{
			m_pTranNodeInfo = pNextNodeInfo;
		}
		if(pNextNodeInfo != NULL)
		{
			pNextNodeInfo->pPreObj = pPreNodeInfo;
		}
		//释放当前节点
		delete[] pNodeInfo;
		break;
	}
	//释放互斥同步对象
	//ReleaseSRWLockExclusive(&m_pTranSRWLock);
	pthread_mutex_unlock(&m_pTranSRWLock);
	return 0;
}
//解析服务器回应的验证信息
bool CRtspClient::handleAuthenticationFailure(char const* paramsStr)
{
	if (paramsStr == NULL)
	{
		return false; // There was no "WWW-Authenticate:" header; we can't proceed.
	}

	// Fill in "fCurrentAuthenticator" with the information from the "WWW-Authenticate:" header:
	char* realm = strDupSize(paramsStr);
	char* nonce = strDupSize(paramsStr);
	bool  success = true;
	if (sscanf(paramsStr, "Digest realm=\"%[^\"]\", nonce=\"%[^\"]\"", realm, nonce) == 2)
	{
		m_pOurAuthenticator->setRealmAndNonce(realm, nonce);
	}
	else if (sscanf(paramsStr, "Basic realm=\"%[^\"]\"", realm) == 1)
	{
		m_pOurAuthenticator->setRealmAndNonce(realm, NULL); // Basic authentication
	}
	else
	{
		success = false; // bad "WWW-Authenticate:" header
	}
	delete[] realm; delete[] nonce;

	if ( m_pOurAuthenticator->username() == NULL || m_pOurAuthenticator->password() == NULL)
	{
		// We already had a 'realm', or don't have a username and/or password,
		// so the new "WWW-Authenticate:" header information won't help us.  We remain unauthenticated.
		success = false;
	}

	return success;
}
//生成验证信息
char* CRtspClient::createAuthenticatorString(char const* cmd, char const* url)
{
	//判断是否需要用户验证
	if(m_pOurAuthenticator == NULL)
	{
		return strDup("");
	}
	Authenticator& auth = *m_pOurAuthenticator;
	//不存在验证信息
	if ((auth.realm() == NULL) || (auth.username() == NULL) || (auth.password() == NULL))
	{
		return strDup("");
	}
	// We have a filled-in authenticator, so use it:
	char* authenticatorStr;
	if (auth.nonce() != NULL)
	{ // Digest authentication
		char const* const authFmt =
			"Authorization: Digest username=\"%s\", realm=\"%s\", "
			"nonce=\"%s\", uri=\"%s\", response=\"%s\"\r\n";
		char const* response = auth.computeDigestResponse(cmd, url);
		unsigned int authBufSize = (unsigned int)(strlen(authFmt)
			+ strlen(auth.username()) + strlen(auth.realm())
			+ strlen(auth.nonce()) + strlen(url) + strlen(response));
		authenticatorStr = new char[authBufSize];

//		sprintf_s(authenticatorStr, authBufSize, authFmt,
//			auth.username(), auth.realm(),
//			auth.nonce(), url, response);
		sprintf(authenticatorStr, authFmt,
					auth.username(), auth.realm(),
					auth.nonce(), url, response);
		auth.reclaimDigestResponse(response);
	}
	else
	{ // Basic authentication
		char const* const authFmt = "Authorization: Basic %s\r\n";

		unsigned int usernamePasswordLength = (unsigned int)(strlen(auth.username()) + 1 + strlen(auth.password()));
		char* usernamePassword = new char[usernamePasswordLength+1];
		//sprintf_s(usernamePassword, usernamePasswordLength+1, "%s:%s", auth.username(), auth.password());
		sprintf(usernamePassword, "%s:%s", auth.username(), auth.password());

		char* response = base64Encode(usernamePassword, usernamePasswordLength);
		unsigned int const authBufSize = (unsigned int)(strlen(authFmt) + strlen(response) + 1);
		authenticatorStr = new char[authBufSize];
		//sprintf_s(authenticatorStr, authBufSize, authFmt, response);
		sprintf(authenticatorStr, authFmt, response);
		delete[] response; delete[] usernamePassword;
	}

	return authenticatorStr;
}


int8 CRtspClient::parseSDPLine_s(char const* sdpLine)
{
	// Check for "s=<session name>" line
	char* buffer = strDupSize(sdpLine);
	char  parseSuccess = 0;

	if (sscanf(sdpLine, "s=%[^\r\n]", buffer) == 1)
		parseSuccess = 1;
	delete[] buffer;

	return parseSuccess;
}

int8 CRtspClient::parseSDPLine_i(char const* sdpLine)
{
	// Check for "i=<session description>" line
	char* buffer = strDupSize(sdpLine);
	char  parseSuccess = 0;

	if (sscanf(sdpLine, "i=%[^\r\n]", buffer) == 1)
		parseSuccess = 1;
	delete[] buffer;

	return parseSuccess;
}

int8 CRtspClient::parseSDPLine_c(char const* sdpLine)
{
	// Check for "c=IN IP4 <connection-endpoint>"
	// or "c=IN IP4 <connection-endpoint>/<ttl+numAddresses>"
	// (Later, do something with <ttl+numAddresses> also #####)
	char* buffer = strDupSize(sdpLine); // ensures we have enough space
	char  parseSuccess = 0;

	if (sscanf(sdpLine, "c=IN IP4 %[^/\r\n]", buffer) == 1)
	{
		parseSuccess = 1;
	}
	delete[] buffer;

	return parseSuccess;
}

int8 CRtspClient::parseSDPLine_b(char const* sdpLine)
{
	char  parseSuccess = 0;
	//网络带宽
	unsigned int fBandwidth;
	// Check for "b=<bwtype>:<bandwidth>" line
	// RTP applications are expected to use bwtype="AS"
	if(sscanf(sdpLine, "b=AS:%u", &fBandwidth) == 1)
	{
		parseSuccess = 1;
	}

	return parseSuccess;
}

int8 CRtspClient::parseSDPAttribute_type(char const* sdpLine)
{
	// Check for a "a=type:broadcast|meeting|moderated|test|H.332|recvonly" line:
	char parseSuccess = 0;

	char* buffer = strDupSize(sdpLine);
	if (sscanf(sdpLine, "a=type: %[^ ]", buffer) == 1)
		parseSuccess = 1;
	delete[] buffer;

	return parseSuccess;
}

int8 CRtspClient::parseSDPAttribute_rtpmap(char const* sdpLine)
{
	// Check for a "a=rtpmap:<fmt> <codec>/<freq>" line:
	// (Also check without the "/<freq>"; RealNetworks omits this)
	// Also check for a trailing "/<numChannels>".
	char parseSuccess = 0;

	unsigned rtpmapPayloadFormat;
	char* codecName = strDupSize(sdpLine); // ensures we have enough space
	unsigned rtpTimestampFrequency = 0;
	unsigned numChannels = 1;
	if (	 sscanf(sdpLine, "a=rtpmap: %u %[^/]/%u/%u", &rtpmapPayloadFormat, codecName, &rtpTimestampFrequency, &numChannels) == 4
		|| sscanf(sdpLine, "a=rtpmap: %u %[^/]/%u",    &rtpmapPayloadFormat, codecName, &rtpTimestampFrequency) == 3
		|| sscanf(sdpLine, "a=rtpmap: %u %s",  		 &rtpmapPayloadFormat, codecName) == 2)
	{
		// This "rtpmap" matches our payload format, so set our
		// codec name and timestamp frequency:
		// (First, make sure the codec name is upper case)
		for (char* p = codecName; *p != '\0'; ++p) *p = (char)toupper(*p);
		//处理H264类型
		//if(_strnicmp(codecName,"H264",4) == 0)
		if(strncasecmp(codecName,"H264",4) == 0)
		{
			parseSuccess = 1;
		}
	}
	delete[] codecName;

	return parseSuccess;
}
int8 CRtspClient::parseSDPAttribute_rtcpmux(char const* sdpLine)
{
	if (strncmp(sdpLine, "a=rtcp-mux", 10) == 0)
	{
		m_bMultiplexRTCPWithRTP = true;
		return 1;
	}
	return 0;
}
int8 CRtspClient::parseSDPAttribute_control(char const* sdpLine)
{
	// Check for a "a=control:<control-path>" line:
	char parseSuccess = 0;

	char* controlPath = strDupSize(sdpLine); // ensures we have enough space
	if (sscanf(sdpLine, "a=control: %s", controlPath) == 1)
	{
		if(controlPath[0] != '*')
		{
			//判断a=control:rtsp://172.38.230.1:554/stream1/trackID=2
			//if(_strnicmp(controlPath,this->m_chBaseURL, strlen(this->m_chBaseURL)) == 0)
			if(strncasecmp(controlPath,this->m_chBaseURL, strlen(this->m_chBaseURL)) == 0)
			{
				strcpy(m_chTrackID,controlPath+strlen(this->m_chBaseURL));
			}
			else
			{
				strcpy(m_chTrackID,controlPath);
			}
		}
		parseSuccess = 1;
	}
	delete[] controlPath;

	return parseSuccess;
}

int8 CRtspClient::parseSDPAttribute_range(char const* sdpLine)
{
	// Check for a "a=range:npt=<startTime>-<endTime>" line:
	// (Later handle other kinds of "a=range" attributes also???#####)
	char parseSuccess = 0;
	double playStartTime;
	double playEndTime;

	if(sscanf(sdpLine, "a=range: npt = %lg - %lg", &playStartTime, &playEndTime) == 2)
	{
		parseSuccess = 1;
	}

	return parseSuccess;
}
int8 CRtspClient::parseSDPAttribute_source_filter(char const* sdpLine)
{
	// Check for a "a=source-filter:incl IN IP4 <something> <source>" line.
	// Note: At present, we don't check that <something> really matches
	// one of our multicast addresses.  We also don't support more than
	// one <source> #####
	char parseSuccess = 0; // until we succeed
	char* sourceName = strDupSize(sdpLine); // ensures we have enough space
	if (sscanf(sdpLine, "a=source-filter: incl IN IP4 %*s %s", sourceName) != 1)
	{
		parseSuccess = 1;
	}
	delete[] sourceName;

	return parseSuccess;
}

int8 CRtspClient::parseSDPAttribute_x_dimensions(char const* sdpLine)
{
	// Check for a "a=x-dimensions:<width>,<height>" line:
	char parseSuccess = 0;
	int width, height;

	if (sscanf(sdpLine, "a=x-dimensions:%d,%d", &width, &height) == 2)
	{
		parseSuccess = 1;
	}

	return parseSuccess;
}

int8 CRtspClient::parseSDPAttribute_framerate(char const* sdpLine)
{
	// Check for a "a=framerate: <fps>" or "a=x-framerate: <fps>" line:
	char parseSuccess =0;
	float frate;
	int rate;

	if (sscanf(sdpLine, "a=framerate: %f", &frate) == 1 || sscanf(sdpLine, "a=framerate:%f", &frate) == 1)
	{
		parseSuccess = 1;
	}
	else if (sscanf(sdpLine, "a=x-framerate: %d", &rate) == 1)
	{
		parseSuccess = 1;
	}

	return parseSuccess;
}

//解析SDP参数
bool CRtspClient::initializeWithSDP(char const* sdpDescription)
{
	//判断SDP参数串是否为空
	if (sdpDescription == NULL)
	{
		return (false);
	}
	//记录sdp信息
	char const* sdpLine = sdpDescription;
	//下一行sdp信息
	char const* nextSDPLine;
	//退出标志 找到视频通道后退出
	char bExit = 0;
	while (1)
	{
		//解析SDP行信息
		if (!parseSDPLine(sdpLine, nextSDPLine)) return (false);
		//##### We should really check for:
		// - "a=control:" attributes (to set the URL for aggregate control)
		// - the correct SDP version (v=0)
		//解析到媒体信息，退出循环
		if (sdpLine[0] == 'm') break;
		//获取下一行信息
		sdpLine = nextSDPLine;
		//没有找到媒体信息，退出循环
		if (sdpLine == NULL) break; // there are no m= lines at all
		// Check for various special SDP lines that we understand:
		//解析会话名称，s=<会话名字>
		if (parseSDPLine_s(sdpLine)) continue;
		//解析会话描述信息，i=<会话描述信息>
		if (parseSDPLine_i(sdpLine)) continue;
		//解析连接数据<网络类型><地址类型><链接地址>
		if (parseSDPLine_c(sdpLine)) continue;
		//解析属性a=control:*,未知属性？
		if (parseSDPAttribute_control(sdpLine)) continue;
		//解析播放起止时间，a=range:npt=<起止时间>-<结束时间>
		if (parseSDPAttribute_range(sdpLine)) continue;
		//解析会议类型，a=type:<会议类型>
		if (parseSDPAttribute_type(sdpLine)) continue;
		//源filter
		if (parseSDPAttribute_source_filter(sdpLine)) continue;
	}

	while (sdpLine != NULL)
	{
		// Parse the line as "m=<medium_name> <client_portNum> RTP/AVP <fmt>"
		// or "m=<medium_name> <client_portNum>/<num_ports> RTP/AVP <fmt>"
		// (Should we be checking for >1 payload format number here?)#####
		//媒体名称
		char* mediumName = strDupSize(sdpLine); // ensures we have enough space
		//传输协议名称
		char const* protocolName = NULL;
		//格式类型
		unsigned payloadFormat;
		//客户端端口
		unsigned short clientPortNum;
		//解析协议类型，m=<媒体类型><端口><协议><格式类型>
		if ((  sscanf(sdpLine, "m=%s %hu RTP/AVP %u", mediumName, &clientPortNum, &payloadFormat) == 3
			|| sscanf(sdpLine, "m=%s %hu/%*u RTP/AVP %u", mediumName, &clientPortNum, &payloadFormat) == 3)
			&& payloadFormat <= 127)
		{
				protocolName = "RTP";
		}
		else if ((sscanf(sdpLine, "m=%s %hu UDP %u", mediumName, &clientPortNum, &payloadFormat) == 3
			||	  sscanf(sdpLine, "m=%s %hu udp %u", mediumName, &clientPortNum, &payloadFormat) == 3
			||	  sscanf(sdpLine, "m=%s %hu RAW/RAW/UDP %u", mediumName, &clientPortNum, &payloadFormat) == 3)
			&&	  payloadFormat <= 127)
		{
				// This is a RAW UDP source
				protocolName = "UDP";
		}
		else
		{
			// This "m=" line is bad; output an error message saying so:
			char* sdpLineStr;
			if (nextSDPLine == NULL)
			{
				sdpLineStr = (char*)sdpLine;
			}
			else
			{
				sdpLineStr = strDup(sdpLine);
				sdpLineStr[nextSDPLine-sdpLine] = '\0';
			}
			//envir() << "Bad SDP \"m=\" line: " <<  sdpLineStr << "\n";
			if (sdpLineStr != (char*)sdpLine) delete[] sdpLineStr;

			delete[] mediumName;

			// Skip the following SDP lines, up until the next "m=":
			while (1)
			{
				sdpLine = nextSDPLine;
				if (sdpLine == NULL) break; // we've reached the end
				if (!parseSDPLine(sdpLine, nextSDPLine)) return false;

				if (sdpLine[0] == 'm') break; // we've reached the next subsession
			}
			continue;
		}
		//保存格式类型
		//if(_strnicmp(mediumName,"video",5) == 0)
		if(strncasecmp(mediumName,"video",5) == 0)
		{
			m_nRTPPayloadFormat = (unsigned char)payloadFormat;
			bExit = 1;
		}
		//保存媒体类型
		delete[] mediumName;
		// Process the following SDP lines, up until the next "m=":
		while (1)
		{
			sdpLine = nextSDPLine;
			//解析到会话尾部，结束循环
			if (sdpLine == NULL) break; // we've reached the end
			if (!parseSDPLine(sdpLine, nextSDPLine)) return false;

			//解析到另一个媒体描述
			if (sdpLine[0] == 'm') break; // we've reached the next subsession
			// Check for various special SDP lines that we understand:
			//解析连接数据:c=<网络类型><地址类型><链接地址>
			if (parseSDPLine_c(sdpLine)) continue;
			//解析带宽:b=<带宽类型>:<带宽值>
			if (parseSDPLine_b(sdpLine)) continue;
			//解析rtp和rtcp是否为混合发送
			if (parseSDPAttribute_rtcpmux(sdpLine)) continue;
			//解析媒体负载匹配属性:a=rtpmap:<匹配rtp负载编号><编码格式><始终率>
			if (parseSDPAttribute_rtpmap(sdpLine)) continue;
			//解析通道ID:a=control:<通道ID>
			if (parseSDPAttribute_control(sdpLine)) continue;
			//解析播放起止时间，a=range:npt=<起止时间>-<结束时间>
			if (parseSDPAttribute_range(sdpLine)) continue;
			//解析视频源的地址
			if (parseSDPAttribute_source_filter(sdpLine)) continue;
			//解析图像的宽、高属性：a=x-dimensions:<宽><高>
			if (parseSDPAttribute_x_dimensions(sdpLine)) continue;
			//解析媒体的码率
			if (parseSDPAttribute_framerate(sdpLine)) continue;
			// (Later, check for malformed lines, and other valid SDP lines#####)
		}
		//已找到视频通道 退出函数
		if(bExit == 1) break;
	}
	return true;
}
//TCP模式数据传输
int32 CRtspClient::ReadRTPoverTCP()
{
	do
	{
		//读取标示符
		if (m_fTCPReadingState != AWAITING_PACKET_DATA)
		{
			//接收TCP数据
			m_nRecvLen = TCP_Recv(m_nRtspSocket,m_chRecvBuf,1,20000);
			if(m_nRecvLen != 1)
				return 0;
		}
		//根据当前状态处理数据
		switch (m_fTCPReadingState)
		{
		case AWAITING_DOLLAR:
			{
				if (m_chRecvBuf[0] == '$')
					m_fTCPReadingState = AWAITING_STREAM_CHANNEL_ID;
				break;
			}
		case AWAITING_STREAM_CHANNEL_ID:
			{
				m_fStreamChannelId = (BYTE)m_chRecvBuf[0];
				if (m_fStreamChannelId == m_nRtpChannelId || m_fStreamChannelId == m_nRtcpChannelId)
					m_fTCPReadingState = AWAITING_SIZE1;
				else
					m_fTCPReadingState = AWAITING_DOLLAR;
				break;
			}
		case AWAITING_SIZE1:
			{
				//字节长度高8位
				m_fSizeByte1 = (uint16)m_chRecvBuf[0];
				m_fTCPReadingState = AWAITING_SIZE2;
				break;
			}
		case AWAITING_SIZE2:
			{
				//数据的长度
				m_fTCPReadSize = (m_fSizeByte1<<8)|(BYTE)m_chRecvBuf[0];
				//修改状态
				m_fTCPReadingState = AWAITING_PACKET_DATA;
				//当前RTP数据长度
				m_nRTPLen = 0;
				break;
			}
		case AWAITING_PACKET_DATA:
			{
				//接收剩余数据
				m_nRecvLen = TCP_Recv(m_nRtspSocket,&m_chRecvBuf[m_nRTPLen],m_fTCPReadSize,20000);
				if(m_nRecvLen > 0)
				{
					m_nRTPLen += m_nRecvLen;
					m_fTCPReadSize -= (uint16)m_nRecvLen;
				}
				//判断数据是否接收完成
				if (m_fTCPReadSize == 0)
				{
					m_fTCPReadingState = AWAITING_DOLLAR;
					//TRACE("the rtp len is %d\r\n",m_nRecvLen);
					return m_nRTPLen;
				}
			}
			break;
		}
	}while(m_nRecvLen > 0);
	//未接受到完整RTP包 返回0
	//TRACE("the rtp len is %d\r\n",m_nRecvLen);
	return m_nRecvLen;
}

static void decodeURL(char* url)
{
	// Replace (in place) any %<hex><hex> sequences with the appropriate 8-bit character.
	char* cursor = url;
	while (*cursor)
	{
		if ((cursor[0] == '%') && cursor[1] && isxdigit(cursor[1]) && cursor[2] && isxdigit(cursor[2]))
		{
			// We saw a % followed by 2 hex digits, so we copy the literal hex value into the URL, then advance the cursor past it:
			char hex[3];
			hex[0] = cursor[1];
			hex[1] = cursor[2];
			hex[2] = '\0';
			*url++ = (char)strtol(hex, NULL, 16);
			cursor += 3;
		}
		else
		{
			// Common case: This is a normal character or a bogus % expression, so just copy it
			*url++ = *cursor++;
		}
	}
	*url = '\0';
}
//解析RTSP请求命令
bool parseRTSPRequestString(char const* reqStr,
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
							unsigned int& contentLength)
{
	unsigned int  i;
	for (i = 0; i < reqStrSize; ++i)
	{
		char c = reqStr[i];
		if (!(c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\0')) break;
	}
	if (i == reqStrSize)
	{
		return false;
	}

	bool parseSucceeded = false;
	unsigned i1 = 0;
	for (; i1 < resultCmdNameMaxSize-1 && i < reqStrSize; ++i,++i1)
	{
		char c = reqStr[i];
		if (c == ' ' || c == '\t')
		{
			parseSucceeded = true;
			break;
		}
		resultCmdName[i1] = c;
	}
	resultCmdName[i1] = '\0';
	if (!parseSucceeded)
	{
		return false;
	}

	// Skip over the prefix of any "rtsp://" or "rtsp:/" URL that follows:
	unsigned int j = i+1;
	// skip over any additional white space
	while (j < reqStrSize && (reqStr[j] == ' ' || reqStr[j] == '\t')) ++j;
	for (; (int)j < (int)(reqStrSize-8); ++j)
	{
		if (   (reqStr[j] == 'r' || reqStr[j] == 'R')
			&& (reqStr[j+1] == 't' || reqStr[j+1] == 'T')
			&& (reqStr[j+2] == 's' || reqStr[j+2] == 'S')
			&& (reqStr[j+3] == 'p' || reqStr[j+3] == 'P')
			&& reqStr[j+4] == ':' && reqStr[j+5] == '/')
		{
			j += 6;
			if (reqStr[j] == '/')
			{
				// This is a "rtsp://" URL; skip over the host:port part that follows:
				++j;
				while (j < reqStrSize && reqStr[j] != '/' && reqStr[j] != ' ') ++j;
			}
			else
			{
				// This is a "rtsp:/" URL; back up to the "/":
				--j;
			}
			i = j;
			break;
		}
	}
	// Look for the URL suffix (before the following "RTSP/"):
	parseSucceeded = false;
	for (unsigned k = i+1; (int)k < (int)(reqStrSize-5); ++k)
	{
		if (reqStr[k] == 'R' && reqStr[k+1] == 'T' &&
			reqStr[k+2] == 'S' && reqStr[k+3] == 'P' && reqStr[k+4] == '/')
		{
				while (--k >= i && reqStr[k] == ' ') {} // go back over all spaces before "RTSP/"
				unsigned k1 = k;
				while (k1 > i && reqStr[k1] != '/') --k1;

				// ASSERT: At this point
				//   i: first space or slash after "host" or "host:port"
				//   k: last non-space before "RTSP/"
				//   k1: last slash in the range [i,k]

				// The URL suffix comes from [k1+1,k]
				// Copy "resultURLSuffix":
				unsigned int n = 0, k2 = k1+1;
				if (k2 <= k)
				{
					if (k - k1 + 1 > resultURLSuffixMaxSize) return false; // there's no room
					while (k2 <= k) resultURLSuffix[n++] = reqStr[k2++];
				}
				resultURLSuffix[n] = '\0';

				// The URL 'pre-suffix' comes from [i+1,k1-1]
				// Copy "resultURLPreSuffix":
				n = 0; k2 = i+1;
				if (k2+1 <= k1)
				{
					if (k1 - i > resultURLPreSuffixMaxSize) return false; // there's no room
					while (k2 <= k1-1) resultURLPreSuffix[n++] = reqStr[k2++];
				}
				resultURLPreSuffix[n] = '\0';
				decodeURL(resultURLPreSuffix);

				i = k + 7; // to go past " RTSP/"
				parseSucceeded = true;
				break;
		}
	}
	if (!parseSucceeded) return false;

	// Look for "CSeq:" (mandatory, case insensitive), skip whitespace,
	// then read everything up to the next \r or \n as 'CSeq':
	parseSucceeded = false;
	for (j = i; (int)j < (int)(reqStrSize-5); ++j)
	{
		//if (_strnicmp("CSeq:", &reqStr[j], 5) == 0)
		if (strncasecmp("CSeq:", &reqStr[j], 5) == 0)
		{
			j += 5;
			while (j < reqStrSize && (reqStr[j] ==  ' ' || reqStr[j] == '\t')) ++j;
			unsigned n;
			for (n = 0; n < resultCSeqMaxSize-1 && j < reqStrSize; ++n,++j) {
				char c = reqStr[j];
				if (c == '\r' || c == '\n')
				{
					parseSucceeded = true;
					break;
				}

				resultCSeq[n] = c;
			}
			resultCSeq[n] = '\0';
			break;
		}
	}
	if (!parseSucceeded)
	{
		return false;
	}

	// Look for "Session:" (optional, case insensitive), skip whitespace,
	// then read everything up to the next \r or \n as 'Session':
	resultSessionIdStr[0] = '\0'; // default value (empty string)
	for (j = i; (int)j < (int)(reqStrSize-8); ++j)
	{
		//if (_strnicmp("Session:", &reqStr[j], 8) == 0)
		if (strncasecmp("Session:", &reqStr[j], 8) == 0)
		{
			j += 8;
			while (j < reqStrSize && (reqStr[j] ==  ' ' || reqStr[j] == '\t')) ++j;
			unsigned int n;
			for (n = 0; n < resultSessionIdStrMaxSize-1 && j < reqStrSize; ++n,++j)
			{
				char c = reqStr[j];
				if (c == '\r' || c == '\n')
				{
					break;
				}

				resultSessionIdStr[n] = c;
			}
			resultSessionIdStr[n] = '\0';
			break;
		}
	}

	// Also: Look for "Content-Length:" (optional, case insensitive)
	contentLength = 0; // default value
	for (j = i; (int)j < (int)(reqStrSize-15); ++j)
	{
		//if (_strnicmp("Content-Length:", &(reqStr[j]), 15) == 0)
		if (strncasecmp("Content-Length:", &(reqStr[j]), 15) == 0)
		{
			j += 15;
			while (j < reqStrSize && (reqStr[j] ==  ' ' || reqStr[j] == '\t')) ++j;
			unsigned int num;
			if (sscanf(&reqStr[j], "%u", &num) == 1)
			{
				contentLength = num;
			}
		}
	}
	return true;
}
//重新发送命令
void CRtspClient::resendCommand()
{
	//发送请求可用操作命令
	if (strcmp(m_pWaitCommandName, "OPTIONS") == 0)
	{
		sendOPTIONCmd();
	}
	//发送请求资源描述命令
	else if (strcmp(m_pWaitCommandName, "DESCRIBE") == 0)
	{
		sendDESCRIBECmd();
	}
	//发送建立连接命令
	else if (strcmp(m_pWaitCommandName, "SETUP") == 0)
	{
		sendSETUPCmd();
	}
	//发送开始播放命令
	else if (strcmp(m_pWaitCommandName, "PLAY") == 0)
	{
		sendPLAYCmd();
	}
	//发送断开连接命令
	else if (strcmp(m_pWaitCommandName, "TEARDOWN") == 0)
	{
		sendTEARDOWNCmd();
	}
}
//解析请求命令
void CRtspClient::handleIncomingRequest()
{
	// Parse the request string into command name and 'CSeq', then 'handle' the command (by responding that we don't support it):
	char cmdName[RTSP_PARAM_STRING_MAX];
	char urlPreSuffix[RTSP_PARAM_STRING_MAX];
	char urlSuffix[RTSP_PARAM_STRING_MAX];
	char cseq[RTSP_PARAM_STRING_MAX];
	char sessionId[RTSP_PARAM_STRING_MAX];
	unsigned contentLength;
	if (!parseRTSPRequestString(m_pResponseBuffer, m_nResponseBytesAlreadySeen,
		cmdName, sizeof cmdName,
		urlPreSuffix, sizeof urlPreSuffix,
		urlSuffix, sizeof urlSuffix,
		cseq, sizeof cseq,
		sessionId, sizeof sessionId,
		contentLength))
	{
			return;
	}
}
//解析回应码
bool CRtspClient::parseResponseCode(char const* line, unsigned& responseCode, char const*& responseString)
{
  if (sscanf(line, "RTSP/%*s%u", &responseCode) != 1)
  {
	  return false;
  }
  responseString = line;
  while (responseString[0] != '\0' && responseString[0] != ' '  && responseString[0] != '\t')
  {
	  ++responseString;
  }

  while (responseString[0] != '\0' && (responseString[0] == ' '  || responseString[0] == '\t'))
  {
	  ++responseString;
  }

  return true;
}

//处理SETUP命令的回应
bool CRtspClient::handleSETUPResponse(char const* sessionParamsStr, char const* transportParamsStr)
{
	char* sessionId = new char[m_nResponseBufferSize];
	bool success = false;
	do
	{
		// Check for a session id:
		if (sessionParamsStr == NULL || sscanf(sessionParamsStr, "%[^;]", sessionId) != 1)
		{
			break;
		}
		//保存session信息
		//strcpy_s(m_chSessionId, 200, sessionId);
		strcpy(m_chSessionId, sessionId);

		// Also look for an optional "; timeout = " parameter following this:
		char const* afterSessionId = sessionParamsStr + strlen(sessionId);
		int timeoutVal;
		if (sscanf(afterSessionId, "; timeout = %d", &timeoutVal) == 1)
		{
			m_nSessionTimeout = timeoutVal;
		}

		// Parse the "Transport:" header parameters:
		char sourceAddrStr[64]={0};
		char destinationAddrStr[64]={0};
		unsigned short serverPortNum;
		unsigned char rtpChannelId, rtcpChannelId;
		if (!parseTransportParams(transportParamsStr, sourceAddrStr, destinationAddrStr, serverPortNum, rtpChannelId, rtcpChannelId))
		{
			break;
		}
		//视频回话源IP地址
		//strcpy_s(m_chSourceAddr, 64, sourceAddrStr);
		strcpy(m_chSourceAddr,  sourceAddrStr);
		m_nSourceAddr = inet_addr(m_chSourceAddr);
		//视频回话目的IP地址
		//strcpy_s(m_chDestinationAddr, 64, destinationAddrStr);
		strcpy(m_chDestinationAddr, destinationAddrStr);
		m_nDestinationAddr = inet_addr(m_chDestinationAddr);
		//源RTP数据端口
		m_nServerRTPPort = serverPortNum;
		//源RTCP数据端口
		m_nServerRTCPPort= serverPortNum + 1;
		//保存视频通道ID
		m_nRtpChannelId = rtpChannelId;
		m_nRtcpChannelId = rtcpChannelId;

		success = true;
	} while (0);

	delete[] sessionId;
	return success;
}
//处理PLAY命令的回应
bool CRtspClient::handlePLAYResponse(char const* , char const* , char const* )
{
	//接收到开始播放的回应 开始发送RTCP报告
	m_bEnableRTCPReports = true;
	m_nVideoFlag = 5;
	//返回成功
	return true;
}
//解析RTSP回应数据
void CRtspClient::handleResponseBytes(int newBytesRead)
{
	do
	{
		//接收到RTSP回应数据 并且数据未超出长度
		if (newBytesRead >= 0 && (unsigned)newBytesRead < m_nResponseBufferBytesLeft)
			break;
		//复位缓存的RTSP数据缓冲区
		m_nResponseBytesAlreadySeen = 0;
		m_nResponseBufferBytesLeft = m_nResponseBufferSize;
		//返回
		return;
	} while (0);

	//修改RTSP命令缓冲区的状态
	m_nResponseBufferBytesLeft  -= newBytesRead;
	m_nResponseBytesAlreadySeen += newBytesRead;
	//添加结束符
	m_pResponseBuffer[m_nResponseBytesAlreadySeen] = '\0';

	//解析RTSP命令后 剩余长度
	unsigned int numExtraBytesAfterResponse = 0;
	//解析回应状态标志
	bool responseSuccess = false;
	do
	{
		//解析RTSP头信息标志
		bool endOfHeaders = false;
		char const* ptr = m_pResponseBuffer;
		if (m_nResponseBytesAlreadySeen > 3)
		{
			char const* const ptrEnd = &m_pResponseBuffer[m_nResponseBytesAlreadySeen-3];
			while (ptr < ptrEnd)
			{
				//判断是否为一行的结束标志
				if (*ptr++ == '\r' && *ptr++ == '\n' && *ptr++ == '\r' && *ptr++ == '\n')
				{
					endOfHeaders = true;
					break;
				}
			}
		}
		//解析头失败 直接返回
		if (!endOfHeaders)
		{
			return;
		}

		char* headerDataCopy = NULL;
		unsigned int responseCode = 200;
		char const* responseStr = NULL;
		char foundRequest = 0;
		char const* sessionParamsStr = NULL;
		char const* transportParamsStr = NULL;
		char const* scaleParamsStr = NULL;
		char const* rangeParamsStr = NULL;
		char const* rtpInfoParamsStr = NULL;
		char const* wwwAuthenticateParamsStr = NULL;
		char const* publicParamsStr = NULL;
		char* bodyStart = NULL;
		unsigned numBodyBytes = 0;
		responseSuccess = false;
		do
		{
			headerDataCopy = new char[m_nResponseBufferSize];
			strncpy(headerDataCopy, m_pResponseBuffer, m_nResponseBytesAlreadySeen);
			headerDataCopy[m_nResponseBytesAlreadySeen] = '\0';

			char* lineStart = headerDataCopy;
			char* nextLineStart = getLine(lineStart);
			//解析RTSP回应码
			if (!parseResponseCode(lineStart, responseCode, responseStr))
			{
				// This does not appear to be a RTSP response; perhaps it's a RTSP request instead?
				handleIncomingRequest();
				break; // we're done with this data
			}

			// Scan through the headers, handling the ones that we're interested in:
			bool reachedEndOfHeaders;
			unsigned cseq = 0;
			unsigned contentLength = 0;

			while (1)
			{
				reachedEndOfHeaders = true; // by default; may get changed below
				lineStart = nextLineStart;
				if (lineStart == NULL) break;

				nextLineStart = getLine(lineStart);
				if (lineStart[0] == '\0') break; // this is a blank line
				reachedEndOfHeaders = false;

				char const* headerParamsStr;
				if (checkForHeader(lineStart, "CSeq:", 5, headerParamsStr))
				{
					if (sscanf(headerParamsStr, "%u", &cseq) != 1 || cseq <= 0)
					{
						break;
					}
					if (m_nWaitCSeq == cseq)
					{
						foundRequest = 1;
					}
					else
					{
						break;
					}
				}
				else if (checkForHeader(lineStart, "Content-Length:", 15, headerParamsStr))
				{
					if (sscanf(headerParamsStr, "%u", &contentLength) != 1)
					{
						break;
					}
				}
				else if (checkForHeader(lineStart, "Content-Base:", 13, headerParamsStr))
				{
					//strcpy_s(m_chBaseURL, 256, headerParamsStr);
					strcpy(m_chBaseURL, headerParamsStr);
				}
				else if (checkForHeader(lineStart, "Session:", 8, sessionParamsStr))
				{
				}
				else if (checkForHeader(lineStart, "Transport:", 10, transportParamsStr))
				{
				}
				else if (checkForHeader(lineStart, "Scale:", 6, scaleParamsStr))
				{
				}
				else if (checkForHeader(lineStart, "Range:", 6, rangeParamsStr))
				{
				}
				else if (checkForHeader(lineStart, "RTP-Info:", 9, rtpInfoParamsStr))
				{
				}
				else if (checkForHeader(lineStart, "WWW-Authenticate:", 17, headerParamsStr))
				{
					// If we've already seen a "WWW-Authenticate:" header, then we replace it with this new one only if
					// the new one specifies "Digest" authentication:
					//if (wwwAuthenticateParamsStr == NULL || _strnicmp(headerParamsStr, "Digest", 6) == 0)
					if (wwwAuthenticateParamsStr == NULL || strncasecmp(headerParamsStr, "Digest", 6) == 0)
					{
						wwwAuthenticateParamsStr = headerParamsStr;
					}
				}
				else if (checkForHeader(lineStart, "Public:", 7, publicParamsStr))
				{
				}
				else if (checkForHeader(lineStart, "Allow:", 6, publicParamsStr))
				{
					// Note: we accept "Allow:" instead of "Public:", so that "OPTIONS" requests made to HTTP servers will work.
				}
				else if (checkForHeader(lineStart, "Location:", 9, headerParamsStr))
				{
					//strcpy_s(m_chBaseURL, 256, headerParamsStr);
					strcpy(m_chBaseURL, headerParamsStr);
				}
			}
			if (!reachedEndOfHeaders)
			{
				break; // an error occurred
			}
			// If we saw a "Content-Length:" header, then make sure that we have the amount of data that it specified:
			unsigned int bodyOffset = nextLineStart == NULL ? m_nResponseBytesAlreadySeen : (unsigned int)(nextLineStart - headerDataCopy);
			bodyStart = &m_pResponseBuffer[bodyOffset];
			numBodyBytes = m_nResponseBytesAlreadySeen - bodyOffset;
			if (contentLength > numBodyBytes)
			{
				// We need to read more data.  First, make sure we have enough space for it:
				unsigned int numExtraBytesNeeded = contentLength - numBodyBytes;
				unsigned int remainingBufferSize = m_nResponseBufferSize - m_nResponseBytesAlreadySeen;
				if (numExtraBytesNeeded > remainingBufferSize)
				{
					break;
				}
				delete[] headerDataCopy;
				return; // We need to read more data
			}

			// We now have a complete response (including all bytes specified by the "Content-Length:" header, if any).
			char* responseEnd = bodyStart + contentLength;
			numExtraBytesAfterResponse = (unsigned int)(&m_pResponseBuffer[m_nResponseBytesAlreadySeen] - responseEnd);
			if (foundRequest != NULL)
			{
				bool needToResendCommand = false; // by default...
				if (responseCode == 200)
				{
					//请求可用操作回应
					if (strcmp(m_pWaitCommandName, "OPTIONS") == 0)
					{
						//发送请求资源描述命令
						sendDESCRIBECmd();
					}
					//请求资源描述回应
					if (strcmp(m_pWaitCommandName, "DESCRIBE") == 0)
					{
						//解析资源描述信息
						if(!initializeWithSDP(bodyStart)) break;
						//发送建立连接命令
						sendSETUPCmd();
					}
					//建立连接命令回应
					else if (strcmp(m_pWaitCommandName, "SETUP") == 0)
					{
						//解析建立连接命令回应
						if (!handleSETUPResponse(sessionParamsStr, transportParamsStr))
						{
							break;
						}
						//发送开始播放命令
						sendPLAYCmd();
						//m_nVideoFlag = 5;

					}
					//开始播放命令回应
					else if (strcmp(m_pWaitCommandName, "PLAY") == 0)
					{
						//解析开始播放命令的回应
						if (!handlePLAYResponse(scaleParamsStr, rangeParamsStr, rtpInfoParamsStr))
						{
							break;
						}
					}
					//断开连接命令回应
					else if (strcmp(m_pWaitCommandName, "TEARDOWN") == 0)
					{
						//if (!handleTEARDOWNResponse()) break;
						int i = 0;
					}
				}
				else if (responseCode == 401 && handleAuthenticationFailure(wwwAuthenticateParamsStr))
				{
					// We need to resend the command, with an "Authorization:" header:
					needToResendCommand = true;
				}
				else if (responseCode == 301 || responseCode == 302)
				{
					//resetTCPSockets(); // because we need to connect somewhere else next
					needToResendCommand = true;
				}
				if (needToResendCommand)
				{
					//复位缓存的RTSP数据缓冲区
					m_nResponseBytesAlreadySeen = 0;
					m_nResponseBufferBytesLeft = m_nResponseBufferSize;
					//重新发送命令
					resendCommand();
					//修改标致
					needToResendCommand = false;
					delete[] headerDataCopy;
					return; // without calling our response handler; the response to the resent command will do that
				}
			}
			responseSuccess = true;
		} while (0);

		if(numExtraBytesAfterResponse == 4)
		{
			numExtraBytesAfterResponse = 0;
		}
		// If we have a handler function for this response, call it.
		// But first, reset our response buffer, in case the handler goes to the event loop, and we end up getting called recursively:
		if (numExtraBytesAfterResponse > 0)
		{
			// An unusual case; usually due to having received pipelined responses.  Move the extra bytes to the front of the buffer:
			char* responseEnd = &m_pResponseBuffer[m_nResponseBytesAlreadySeen - numExtraBytesAfterResponse];

			// But first: A hack to save a copy of the response 'body', in case it's needed below for "resultString":
			numBodyBytes -= numExtraBytesAfterResponse;
			if (numBodyBytes > 0)
			{
				char saved = *responseEnd;
				*responseEnd = '\0';
				//bodyStart = strDup(bodyStart);
				*responseEnd = saved;
			}
			memmove(m_pResponseBuffer, responseEnd, numExtraBytesAfterResponse);
			m_nResponseBytesAlreadySeen = numExtraBytesAfterResponse;
			m_nResponseBufferBytesLeft = m_nResponseBufferSize - numExtraBytesAfterResponse;
			m_pResponseBuffer[numExtraBytesAfterResponse] = '\0';

		}
		else
		{
			//复位缓存的RTSP数据缓冲区
			m_nResponseBytesAlreadySeen = 0;
			m_nResponseBufferBytesLeft = m_nResponseBufferSize;
		}
		delete[] headerDataCopy;
		//if (numExtraBytesAfterResponse > 0 && numBodyBytes > 0) delete[] bodyStart;
	} while (numExtraBytesAfterResponse > 0 && responseSuccess);
}
