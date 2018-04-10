/*
 * CRtspServer.h
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */

#ifndef CRTSPSERVER_H_
#define CRTSPSERVER_H_

#include <pthread.h>
//#include <sys\timeb.h>
#include <vector>
#include <list>

#include "typedef.h"
#include "CVideoInfo.h"
#include "AccessInfo.h"


using namespace std;

//服务器允许的最大连接数 也可作为资源列表中客户端个数
#define nMaxClient		80
//客户端TCP回应的超时时间 单位：ms
#define OVETTIME		5000
//TCP命令回应信息超时时间(注此时间一定要小于 客户端TCP回应的超时时间)
#define CMDOVETTIME		200
//资源重连成功后发送转发控制命令的等待时间 单位： s
#define RECONNSENDTIME	20
//TCP命令缓存区长度
#define TCPBUFLENGTH	4096

// 提取到AccessInfo.h头文件中。
////允许拒绝列表项
//typedef struct
//{
//	char nType; //类型0--允许1--拒绝
//	char startIp[20]; //起始IP地址
//	char endIp[20]; //结束IP地址
//} AccessInfo;

// 提取到CVideoInfo.h头文件中。
//资源信息
//typedef struct
//{
//	int8 chKey[32];		//资源的键值，唯一性
//	int32 nVideoID;		//资源ID编号
//	int8 chVideoSrc[256];		//资源源地址
//	int8 chVideoSrv[256];		//资源服务地址
//	int32 nVideoStatus;	//资源状态
//	int8 nYTType[32];	//云台类型编号
//	int8 chEncFormat[32];	//视频编码格式
//	int8 chSession[32];	//播放时的Session信息
//	int8 chTrack[32];	//播放时的track信息
//	int8 chUser[32];		//访问用户名
//	int8 chPwd[32];		//访问密码
//	int32 nVideoWidth;	//视频画面宽度
//	int32 nVideoHeight;	//视频画面高度
//	int32 nVideoFrames;	//视频帧速
//	int32 nVideoBps;		//视频比特率
//	int32 nStartPort;		//记录节点起始端口
//	int8 bOverTCP;		//是否使用TCP传输视频数据
//	void* pRtspClient;	//与编码器的连接套接字
//} CVideoInfo;

//视频传输模式
/**
 * 视频传输模式枚举
 */
typedef enum StreamingMode
{
	RTP_UDP = 0,	//!< RTP_UDP
	RTP_TCP,    //!< RTP_TCP
	RAW_UDP,    //!< RAW_UDP
	NONSUPPORT  //!< NONSUPPORT
} StreamingMode;

//TCP客户端信息结构体 - 客户端链表基本元素
/**
 * TCP客户端信息结构提
 */
typedef struct
{
	bool bActive;			//是否为活动状态
	int8 nUserType;		//用户类型
	int8 chClientIP[64];	//客户端IP地址
	uint32 nClientIP;
	uint16 nClientPort;		//客户端端口
	int32 nClientSock;		//网络套接字
	int8 chVideoName[200];	//访问的流名称
	//数据缓冲区
	uint8* pRequestBuffer;	//接收数据缓存区
	uint32 nRequestBufSize;	//接收数据缓冲区大小
	uint32 nBytesAlreay;	//已接收数据的长度
	uint8* pLastCRLF;		//字符串结束位置
	uint32 nBytesLeft;		//剩余空间的大小
	//是否启动转发
	int8 nTransFlag;		//是否已启动转发
	StreamingMode mTranMode;		//传输模式, 目前支持RTP_UDP、TRP_TCP模式
	int8 chSessionID[64];	//Session信息
	uint16 nRtpPort;		//RTP数据通讯的端口号
	uint16 nRtcpPort;		//RTCP数据通讯的端口号
	int32 nRtpChannelId;	//RTP数据通道ID
	int32 nRtcpChannelId;	//RTCP数据通道ID
	//服务端RTP参数
	int32 nRtpSocket;		//服务端发送rtp数据的套接字
	int32 nRtcpSocket;		//服务端发送和接收rtcp数据的套接字
	uint16 nSendRtpPort;	//服务端发送rtp数据的端口号
	uint16 nSendRtcpPort;	//服务端发送和接收rtp数据的端口号
	//
	struct timeval nLkTime;	//网络连接时间
	CVideoInfo* pVideoInfo;	//对应的资源信息
} TCPClientInfo;

//RTSP服务器端类
/**
 * RTSP服务器端类
 */
class CRtspServer
{

//定义变量
public:
	//服务器绑定IP地址
	int8* m_chServerIP;
	//服务器绑定端口
	uint16 m_nServerPort;

	//通讯处理线程句柄，保存启动通讯处理线程时返回的句柄
	HANDLE m_hCommDealThread;
	//通讯处理线程ID，启动通讯线程时获得
	DWORD m_dwCommDealThreadID;
	//通讯处理线程运行状态
	int8 m_nThreadFlag;
	//用于向服务程序发送消息的参数
	DWORD m_nMsgThreadID;
	DWORD m_nMsgID;
private:
	//服务器网络套接字
	int32 m_nServerSocket;
	//协议命令缓冲区
	int8* m_chSendCmd;
//定义函数
public:
	//构造函数
	CRtspServer(int8* chServerIP, uint16 nServerPort, DWORD nThreadID,
			DWORD nMsgID);
	//析构函数
	~CRtspServer(void);
	//启动服务
	int8 StartRun(void);
	//停止服务
	void StopRun(void);
	//添加、修改视频流
	int8 SetStream(int8* chVideoName, CVideoInfo* pInfo);
	//删除视频流
	int8 RemoveStream(int8* chVideoName);
	//根据名字查找视频资源对象
	void* FindVideoByName(int8 const* chVideoName);
	//删除视频资源对应的客户端连接对应关系
	void ModifyClientByName(const CVideoInfo* pVideoInfo);
	//启动指定视频流服务
	int8 StartVideo(int8* chVideoName);
	//停止指定视频流服务
	int8 StopVideo(int8* chVideoName);
	//添加客户端节点
	int8 AddRtspClient(int32 nClientSock, int8 * ClientIP, uint16 nClientPort);
	//移除客户端节点
	int8 RemoveRtspClient(TCPClientInfo* pClientInfo);
	int8 RemoveRtspClient(const int8* chVideoName, const int8* chNodeIP,
			const uint16 nNodePort);

	//通讯处理线程
	static void* CommDealThread(void* pParam);
	//处理TCP通讯数据
	int8 DealTCPData();

	// 定时在线客户端诊断 - 10秒检查一次
	void ClientCheck();

	//函数功能：视频信息拷贝
	int8 videoInfoCpy(CVideoInfo * pDstInfo, CVideoInfo * pSrcInfo);

private:
	// 定时在线客户端诊断时间戳
	struct timeval lastCheckTime;

private:
	//解析接收到的请求命令
	void handleRequestBytes(int newBytesRead);
	//组织请求可用操作命令的回应
	void handleCmd_OPTIONS();
	//组织请求资源描述命令的回应
	void handleCmd_DESCRIBE(char const* urlPreSuffix, char const* urlSuffix,
			char const* fullRequestStr);
	//组织建立连接命令的回应
	void handleCmd_SETUP(char const* urlPreSuffix, char const* urlSuffix,
			char const* fullRequestStr);
	//创建Session后命令的处理
	void handleCmd_withinSession(char const* cmdName, char const* urlPreSuffix,
			char const* urlSuffix, char const* fullRequestStr);
	//开始播放命令的回应
	void handleCmd_PLAY(char const* fullRequestStr);
	//断开连接命令的回应
	void handleCmd_TEARDOWN();

	//组织RTSP回应信息
	void setRTSPResponse(char const* responseStr);
	void setRTSPResponse(char const* responseStr, unsigned long sessionId);
	void setRTSPResponse(char const* responseStr, char const* contentStr);
	void setRTSPResponse(char const* responseStr, unsigned long sessionId,
			char const* contentStr);
	//RTSP命令数据错误的回应
	void handleCmd_bad();
	//RTSP命令不支持的回应
	void handleCmd_notSupported();
	//流未找到的回应
	void handleCmd_notFound();
	//Session未找到的回应
	void handleCmd_sessionNotFound();
	//不支持的传输参数的回应
	void handleCmd_unsupportedTransport();

private:
	//视频信息列表
	// FIXME：stl模板库的链表实现
	//CPtrList* m_pVideoList;
	list<void*>* m_pVideoList;

	//访问视频信息的互斥变量
	//SRWLOCK   m_pVideoSRWLock;
	pthread_rwlock_t m_pVideoSRWLock;

	//当前命令序号指针
	char* m_chCurrentCSeq;

	// FIXME：stl模板库的链表实现
	//保存客户端连接的链表
	//CPtrList* m_pClientList;
	list<void*>* m_pClientList;

	//互斥对象用于处理客户端连接访问时的同步
	//SRWLOCK   m_pClientSRWLock;
	pthread_rwlock_t m_pClientSRWLock;

	//当前处理的客户端
	TCPClientInfo* m_pCurClientInfo;
};

#endif /* CRTSPSERVER_H_ */
