/*
 * CRtspClient.h
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */

#ifndef CRTSPCLIENT_H_
#define CRTSPCLIENT_H_

#include "DigestAuthentication.h"
#include "typedef.h"
#include "Queue.h"
#include "CMyQueue.h"
#include "H264RTP_UnPack.h"

#include <sys/timeb.h>

//是否启动转发线程
#define RTP_THREAD_FLAG
//客户端心跳包发送时间 单位：ms
#define CHECKTIME		1000
//重连时间间隔
#define RECONNECTTIME	2000

#ifdef RTP_THREAD_FLAG
//RTP队列大小
#define RTP_QUEUESIZE	1024
#endif
/********************************************/
//RTCP协议处理中相关结构体定义
#define RTCP_SR   200
#define RTCP_RR   201
#define RTCP_SDES 202
#define RTCP_BYE  203
#define RTCP_APP  204
typedef struct
{
#ifdef WORDS_BIGENDIAN
	unsigned short  version:2;	/* packet type            */
	unsigned short  p:1;		/* padding flag           */
	unsigned short  count:5;	/* varies by payload type */
	unsigned short  pt:8;		/* payload type           */
#else
	unsigned short  count:5;	/* varies by payload type */
	unsigned short  p:1;		/* padding flag           */
	unsigned short  version:2;	/* packet type            */
	unsigned short  pt:8;		/* payload type           */
#endif
	unsigned short  length;		/* packet length          */
} rtcp_common;

typedef struct
{
	uint32         ssrc;
	uint32         ntp_sec;
	uint32         ntp_frac;
	uint32         rtp_ts;
	uint32         sender_pcount;
	uint32         sender_bcount;
} rtcp_sr;

typedef struct
{
	uint32	ssrc;		/* The ssrc to which this RR pertains */
#ifdef WORDS_BIGENDIAN
	uint32	fract_lost:8;
	uint32	total_lost:24;
#else
	uint32	total_lost:24;
	uint32	fract_lost:8;
#endif
	uint32	last_seq;
	uint32	jitter;
	uint32	lsr;
	uint32	dlsr;
} rtcp_rr;

/* SDES packet types... */
typedef enum  {
	RTCP_SDES_END   = 0,
	RTCP_SDES_CNAME = 1,
	RTCP_SDES_NAME  = 2,
	RTCP_SDES_EMAIL = 3,
	RTCP_SDES_PHONE = 4,
	RTCP_SDES_LOC   = 5,
	RTCP_SDES_TOOL  = 6,
	RTCP_SDES_NOTE  = 7,
	RTCP_SDES_PRIV  = 8
} rtcp_sdes_type;

typedef struct {
	uint8		type;		/* type of SDES item              */
	uint8		length;		/* length of SDES item (in bytes) */
	char		data[1];	/* text, not zero-terminated      */
} rtcp_sdes_item;

typedef struct
{
	rtcp_common   common;
	union
	{
		struct
		{
			rtcp_sr		sr;
			rtcp_rr     rr[1];		/* variable-length list */
		} sr;
		struct
		{
			uint32        ssrc;		/* source this RTCP packet is coming from */
			rtcp_rr       rr[1];	/* variable-length list */
		} rr;
		struct rtcp_sdes_t
		{
			uint32	ssrc;
			rtcp_sdes_item 	item[1];	/* list of SDES */
		} sdes;
		struct
		{
			uint32        ssrc[1];	/* list of sources */
			/* can't express the trailing text... */
		} bye;
		struct
		{
			uint32        ssrc;
			uint8         name[4];
			uint8         data[1];
		} app;
	} r;
} rtcp_t;
/********************************************/

//转发信息
typedef struct{
	//目的IP地址
	uint32 nNodeIP;
	//目的端口
	uint16 nNodePort;
	//客户端连接套接字
	int32 nSocket;
	//通道编号
	int8  nChannelId;
	//是否活动主要针对TCP模式
	int8  bActive;
	//上一个节点
	void* pPreObj;
	//下一个节点
	void* pNextObj;
}STranNodeInfo;

//RTP数据信息
typedef struct{
	//数据指针
	void* pData;
	//内存大小
	uint16  nSize;
	//RTP数据长度
	uint16  nLen;
}SRtpData;

//RTSP客户端类
class CRtspClient {
public:
	//构造函数
	CRtspClient(int8* AppInfo,uint32 nType ,const int8 * LocalIP = NULL, int8* chUser = NULL, int8* chPwd = NULL, bool bOverTCP = false);
	//析构函数
	~CRtspClient(void);
	//初始化模块
	int8 Init(const int8* chServerIP, const uint16 nServerPort);
	int8 Init(const int8* chName, const int8* chStreamURL, CQueue* fQueue = NULL);
	int8 Init(const int8* chServerIP, const uint16 nServerPort, const int8* chName, const int8* chStreamURL, CQueue* fQueue = NULL);
	//初始化队列
	int8 InitQueue(unsigned int uMemSize, unsigned int uMaxDataSize, unsigned int uMaxExtaraInfoSize);
	//获取视频数据
	int32 GetVideoData( StQueueData* stVideoData);
	//设置状态上报参数
	void SetReportParam(int32 nVideoID, DWORD nThreadID, UINT nMsgID);
	//启动模块
	int8 StartRun();
	//停止模块
	int8 StopRun();
	//得到视频流状态
	int8 GetVideoStatus();
	//请求可用操作
	int8 sendOPTIONCmd();
	//请求资源描述
	int8 sendDESCRIBECmd();
	//建立连接
	int8 sendSETUPCmd();
	//开始播放
	int8 sendPLAYCmd();
	//断开连接
	int8 sendTEARDOWNCmd();
	//处理线程函数
	//static DWORD __stdcall  CommDealThread(LPVOID pParam);
	static void* CommDealThread(void* pParam);

	//接收处理TCP数据
	int8 DealTCPData();
	//解析TCP包，去除无效部分
	int ParseTCPData();
	//接收处理UDP数据
	int8 DealUDPData();
	//处理RTP数据
	int8 DealRTPData(BYTE* chData, unsigned short nLen);
#ifdef RTP_THREAD_FLAG
	//数据转发线程
	//static DWORD __stdcall  TranDataThread(LPVOID pParam);
	static void* TranDataThread(void* pParam);

	//转发RTP数据
	void TranDataFunc();
#endif
	//请求资源访问地址
	int8 GetStreamURL(int8* chName, int8* chVideoAddr);
	//得到统计数据
	int8 GetDataCheck(double* nRecvPack, double* nRecvLen);
	//添加节点信息
	int32   AddSubNode(const int32 nSocket, const uint32 nNodeIP, const uint16 nNodePort);
	int32   AddSubNode(const int32 nSocket, const uint8 nChannelId);
	//删除节点信息
	int32   RemoveSubNode(const int32 nSocket, const uint32 nNodeIP, const uint16 nNodePort);
	int32   RemoveSubNode(const int32 nSocket, const uint8 nChannelId);
public:
	//通讯处理线程Handle
	HANDLE	m_hCommDealThread;
	//通讯处理线程ID
	DWORD	m_dwCommDealThreadID;
	//通讯处理线程运行状态
	int8	m_nCommDealThreadFlag;
#ifdef RTP_THREAD_FLAG
	//数据转发线程Handle
	HANDLE	m_hTranDataThread;
	//数据转发线程ID
	DWORD	m_dwTranDataThreadID;
	//数据转发线程运行状态
	int8	m_nTranDataThreadFlag;
#endif
	//视频流当前状态 0-断开 1-发送操作请求 2-发送描述请求 3-发送建立连接请求 4-发送播放请求 5-开始播放
	int8	m_nVideoFlag;
private:
	//存储统计结果
	double m_nTotalSuccLen;		//发送成功长度
	double m_nToalSuccPackNum;	//发送成功包数
	double m_nSendFailPackNum;	//发送失败包数
	uint32 m_nFramePerSec;		//每一秒的帧数
public:
	uint32 RTCPRRRespose( char* buffer, int length);	//组织RR回应数据包
	BOOL   Validate_rtcp(int8 *packet, int len);		//验证是否RTCP数据包//生成验证信息
	bool   handleAuthenticationFailure(char const* paramsStr);			//解析服务器回应的验证信息
	int8*  createAuthenticatorString(char const* cmd, char const* url);	//生成验证信息
	int8 SendRecvReport();												//发送接收者报告
	int8 SendBYE();														//发送离开报告
	bool initializeWithSDP(char const* sdpDescription);
	int8 parseSDPLine_s(char const* sdpLine);
	int8 parseSDPLine_i(char const* sdpLine);
	int8 parseSDPLine_c(char const* sdpLine);
	int8 parseSDPLine_b(char const* sdpLine);
	int8 parseSDPAttribute_type(char const* sdpLine);
	int8 parseSDPAttribute_rtpmap(char const* sdpLine);
	int8 parseSDPAttribute_control(char const* sdpLine);
	int8 parseSDPAttribute_rtcpmux(char const* sdpLine);
	int8 parseSDPAttribute_range(char const* sdpLine);
	int8 parseSDPAttribute_source_filter(char const* sdpLine);
	int8 parseSDPAttribute_x_dimensions(char const* sdpLine);
	int8 parseSDPAttribute_framerate(char const* sdpLine);

	//解析RTSP回应数据
	void handleResponseBytes(int newBytesRead);
	//解析RTSP的回应码
	bool parseResponseCode(char const* line, unsigned& responseCode, char const*& responseString);
	//处理SETUP命令的回应
	bool handleSETUPResponse(char const* sessionParamsStr, char const* transportParamsStr);
	//处理PLAY命令的回应
	bool handlePLAYResponse(char const* scaleParamsStr, char const* rangeParamsStr, char const* rtpInfoParamsStr);
	//解析请求命令
	void handleIncomingRequest();
	//重新发送命令
	void resendCommand();
private:
	//客户端类型
	int32 m_nAppType;
	//客户端程序信息
	int8* m_chAppInfo;
	//本地IP地址
	int8* m_chLocalIP;
	//本地TCP端口
	int32 m_nLocalPort;
	//建立TCP连接时返回的套接字
	int32 m_nRtspSocket;
	//服务器IP地址；
	int8* m_chServerIP;
	uint32 m_nServerIP;
	//服务器TCP端口；
	uint16 m_nServerPort;
	//数据发送方的IP和端口
	uint32 m_nFromIP;
	uint16 m_nFromPort;
	//流访问基地址
	int8* m_chBaseURL;
	//用于用户验证
	Authenticator* m_pOurAuthenticator;

	//rtsp缓冲区
	int8* m_pResponseBuffer;
	//rtsp缓冲区总长度
	uint32 m_nResponseBufferSize;
	//rtsp缓冲区占用的长度
	uint32 m_nResponseBytesAlreadySeen;
	//rtsp缓冲区剩余的长度
	uint32 m_nResponseBufferBytesLeft;
	//等待回应的命令序号
	uint32 m_nWaitCSeq;
	//等待回应的命令码
	int8*  m_pWaitCommandName;

	//是否基于TCP传输视频流
	bool  m_bOverTCP;
	//视频通道名称
	int8* m_chTrackID;
	//回话描述信息(Session)
	int8* m_chSessionId;
	//回话超时时间
	int32 m_nSessionTimeout;
	//视频回话源IP地址
	int8* m_chSourceAddr;
	uint32 m_nSourceAddr;
	//视频回话目的IP地址
	int8* m_chDestinationAddr;
	uint32 m_nDestinationAddr;
	//源RTP数据端口
	uint16 m_nServerRTPPort;
	//源RTCP数据端口
	uint16 m_nServerRTCPPort;
	//视频数据接收缓冲区
	int8* m_chRecvBuf;
	//接收数据长度
	int32 m_nRecvLen;
	//当前RTP数据的长度
	int32 m_nRTPLen;

	//保存视频通道ID
	uint8 m_nRtpChannelId;
	uint8 m_nRtcpChannelId;
	//rtp和rtcp是否混合发送
	bool m_bMultiplexRTCPWithRTP;
	//与设备通讯RTP数据的套接字和端口
	int32 m_nRtpSocket;
	uint16 m_nRtpPortNum;
	//与设备通讯RTCP数据的套接字和端口
	int32 m_nRtcpSocket;
	uint16 m_nRtcpPortNum;

	//是否使能发送RTCP报告
	bool  m_bEnableRTCPReports;
	//接收到的RTCP数据长度
	int32 m_nRTCPLen;
	//保存RTCP数据的缓冲区
	int8* m_chRTCPBuf;
	//最后接收RTCP包的时间
	struct timeval m_lastRTCP;
	//接收者报告发送时间间隔,单位：？ 应该是毫秒为单位。
	uint32 m_nReportInterval;

	//发送接收者报告的时间
	//FILETIME m_sLastTime;
	struct timeval m_sLastTime;	// 2017-08-08 linux使用gettimeofday获取系统时间。

	//H264RTP数据解包类
	CH264_Rtp_UnPack* m_pH264RTPunPack;
	//rtp数据包序号
	uint32 m_nRtpSeq;
	//SR的ntp时间中间32位
	uint32 m_nNtpTime;
	uint32 m_nSsrc;
	uint32 m_nCsrc;

	//自定义协议中的包序号
	uint32  m_nOutPackNum;
	//用于检测是否接收到了RTP数据
	uint32  m_nTickCount;
	//心跳监测时间
	struct timeb m_lasttb;
	//当前时间时间
	struct timeb m_curtb;

	//定义视频数据队列
	CQueue *m_pVideoQueue;
	CMyQueue* m_pVideoQueueEx;
	//队列元素结构
	StQueueData *m_stDataSrc;
	//队列剩余空间
	int m_nFreeSize;

	//帧数据长度
	int m_nFrameSize;
	//帧类型
	int m_nFrametype;
	//帧时间戳
	uint32 m_nFrameTs;
	//丢包标志
	uint8  m_nLostFlag;
public:
	//视频流名称
	int8* m_chVideoName;
	//P2P流访问地址
	int8* m_chStreamURL;
	//RTP包负载类型
	uint8 m_nRTPPayloadFormat;
	//是否为第一次启动
	uint8 m_bFirstRun;
private:
	//RTP数据长度的低8位
	uint16 m_fSizeByte1;
	//RTP信息中的长度
	uint16 m_fTCPReadSize;
	//RTP数据通道编号
	uint8  m_fStreamChannelId;
	//RTP接收的当前状态
	enum { AWAITING_DOLLAR, AWAITING_STREAM_CHANNEL_ID, AWAITING_SIZE1, AWAITING_SIZE2, AWAITING_PACKET_DATA } m_fTCPReadingState;
	//TCP模式数据传输
	int32 ReadRTPoverTCP();
private:
	//转发节点信息
	STranNodeInfo* m_pTranNodeInfo;
	//转发节点的计数
	int32 m_nNodeCount;

	//互斥对象用于处理节点添加和删除时的同步
	//SRWLOCK    m_pTranSRWLock;
	pthread_mutex_t m_pTranSRWLock; // 2017-08-08 linux使用pthread库代替。

#ifdef RTP_THREAD_FLAG
	//RTP数据队列
	SRtpData m_pRtpQueue[RTP_QUEUESIZE];
	//RTP队列头指针和尾指针
	uint16 m_nRtpQueueHead;
	uint16 m_nRtpQueueTail;
#endif
	//用于处理转发数据时间间隔
	struct timeval m_nLastTranTime;
	struct timeval m_nNowTranTime;

	//视频服务线程的ID
	DWORD m_nServiceThreadID;
	//向视频服务发送消息的ID
	UINT  m_nServiceMsgID;
	//当前视频编号
	uint16 m_nVideoID;
	//当前视频状态
	uint8  m_nVideoStatus;
	//yxj,解rtcp
	//偏移长度
	unsigned int packLen;
	unsigned int seekFlag;
};


#endif /* CRTSPCLIENT_H_ */
