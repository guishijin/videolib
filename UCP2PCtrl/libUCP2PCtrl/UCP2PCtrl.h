/*
 * UCP2PCtrl.h
 *
 *  Created on: 2017年8月8日
 *      Author: root
 */

#ifndef UCP2PCTRL_H_
#define UCP2PCTRL_H_

#include "typedef.h"
#include "Queue.h"
#include "CVideoInfo.h"
#include "AccessInfo.h"
#include "ERTSPAppType.h"

///////////////////////////////////////////////////////////////////////////////
//																													//
//    初始化  																								//
//																													//
///////////////////////////////////////////////////////////////////////////////

/**
 * P2PCtrl动态库初始化
 * @return true - 成功，false - 失败
 */
bool P2PCtrl_Init();

///////////////////////////////////////////////////////////////////////////////
//																													//
//    客户端  																								//
//																													//
///////////////////////////////////////////////////////////////////////////////

/**
 * 创建一个RTSP的客户端
 * @param chIP 目标服务器的ip
 * @param nPort 目标服务器的端口
 * @param AppInfo 应用程序信息（字符串描述信息）
 * @param nType 客户端节点类型（ERTSPAppType）
 *
 * @return NULL - 创建失败，其他-客户端对象句柄。
 */
void* P2PNode_Create(char* chIP, uint32 const nPort, char* AppInfo,
		uint32 nType);

/**
 * 创建一个RTSP的客户端,扩展函数，支持用户名、密码、rtp over tcp选项。
 * @param chIP 目标服务器的ip
 * @param nPort 目标服务器的端口
 * @param AppInfo 应用程序信息（字符串描述信息）
 * @param nType 客户端节点类型（ERTSPAppType）
 * @param chUser rtsp用户名
 * @param chPwd rtsp密码
 * @param bOverTCP 是否使用 rtp over tcp 0-不是用，1-使用
 *
 * @return NULL - 创建失败，其他-客户端对象句柄。
 */
void* P2PNode_CreateEx(char* chIP, uint32 const nPort, char* AppInfo,
		uint32 nType, char* chUser, char* chPwd, uint8 bOverTCP);

/**
 * 初始化客户端需要的队列
 * @param pNodeID 客户端句柄（P2PNode_Create 或者 P2PNode_CreateEX的返回值）
 * @param uMemSize 队列内存大小
 * @param uMaxDataSize 视频数据最大长度
 * @param uMaxExtaraInfoSize 扩展信息最大长度
 * @return 0-成功，-1-失败
 */
int8 P2PNode_InitQueue(void* const pNodeID, uint32 uMemSize,
		uint32 uMaxDataSize, uint32 uMaxExtaraInfoSize);

/**
 * 获取指定节点的指定流名称的rtsp地址url
 * @param pNodeID 指定的客户端句柄
 * @param chName 流名称
 * @param chVideoAddr 输出参数，rtsp流地址
 * @return 0-成功，-1-失败
 */
int8 P2PNode_GetStreamURL(void* const pNodeID, int8 * const chName,
		int8* chVideoAddr);

/**
 * 开始下载视频数据
 * @param pNodeID RTSP客户端句柄
 * @param chName 流名称
 * @param chAddr rtsp流地址
 * @param fQueue 外部分配的CQueue队列指针，用来接收视频数据。
 * @return 0-成功，-1-失败
 */
int8 P2PNode_Start(void* const pNodeID, int8 * const chName,
		int8 * const chAddr, CQueue* fQueue);

/**
 * 获取RTSP客户端节点当前状态
 * @param pNodeID RTSP客户端节点句柄
 * @param nRecvPack 接收到的Pack包数
 * @param nRecvLen 接收到的数据长度
 * @return -1-失败，0-正在停止，1-正在启动，2-正在运行，3-已经停止。
 */
int8 P2PNode_GetStatus(void* const pNodeID, double *nRecvPack,
		double *nRecvLen);

/**
 * 获取视频数据
 * @param pNodeID RTSP客户端节点句柄
 * @param stVideoData 输出的数据存放指针
 * @return <=0-失败，>0-成功
 */
int32 P2PNode_GetVideoData(void* const pNodeID, StQueueData* stVideoData);

/**
 * 关闭指定视频流下载
 * @param pNodeID RTSP客户端节点句柄
 * @param chName 流名称
 * @return 0-成功， -1-失败
 */
int8 P2PNode_Stop(void* const pNodeID, int8 * const chName);

/**
 * 销毁一个客户端
 * @param pNodeID RTSP客户端节点句柄
 * @return 0-成功，-1-失败
 */
int8 P2PNode_Free(void* const pNodeID);

///////////////////////////////////////////////////////////////////////////////
//																													//
//    服务器端																								//
//																													//
///////////////////////////////////////////////////////////////////////////////

/**
 * 创建一个P2PRTSPServer对象实例
 * @param chServerIP 需要绑定的服务IP
 * @param nServerPort 需要绑定的服务端口
 * @param nThreadID 接收消息的线程id
 * @param nMsgID 监听的消息类型id
 * @return NULL-失败，其他-对象实例句柄
 */
void* P2PSvr_Create(int8* chServerIP, uint16 nServerPort, DWORD nThreadID,
		DWORD nMsgID);

/**
 * 启动RTSP服务器
 * @param pSrvID 服务器实例句柄
 * @param appInfo 应用信息，预留，NULL。
 * @return 0-成功，-1-失败
 */
int8 P2PSvr_Start(void* const pSrvID, int8* appInfo);

/**
 * 向服务器中添加一个视频流
 * @param pSrvID 服务器句柄
 * @param chName 流名称
 * @param pInfo 视频流描述信息
 * @return 0-成功，1-失败
 */
int8 P2PSvr_AddVideo(void* const pSrvID, int8* chName,
		CVideoInfo* pInfo);

/**
 * 从服务器中删除一个视频流
 * @param pSrvID 服务器句柄
 * @param chName 流名称
 * @return 0-成功，1-失败
 */
int8 P2PSvr_RemoveVideo(void* const pSrvID, int8* chName);

/**
 * 启动指定的视频流，名称未指定时启动所有视频流。
 * @param pSrvID 服务器句柄
 * @param chName 流名称
 * @return 0-成功，1-失败
 */
int8 P2PSvr_StartVideo(void* const pSrvID, int8* chName);

/**
 * 停止指定视频流服务，名称未指定时，停止所有视频流服务。
 * @param pSrvID 服务器句柄
 * @param chName 流名称
 * @return 0-成功，1-失败
 */
int8 P2PSvr_StopVideo(void* const pSrvID, int8* chName);

/**
 * 移除客户端节点
 * @param pSrvID 服务器句柄
 * @param chName 流名称
 * @param chNodeIP 客户端节点的目标IP
 * @param nNodePort 客户端节点的目标端口
 *
 * @return 0-成功，1-失败
 */
int8 P2PSvr_RemoveNode(void* const pSrvID, int8* chName, int8* chNodeIP,
		uint16 nNodePort);

/**
 * 停止模块服务
 * @param pSrvID 服务器句柄
 * @return 0-成功，1-失败
 */
int8 P2PSvr_Stop(void* const pSrvID);

/**
 * 停止指定模块服务器,释放资源并销毁服务模块
 * @param pSrvID 服务器句柄
 * @return 0-成功，1-失败
 */
int32 P2PSvr_Destroy(void* const pSrvID);

/**
 * NOT TO USE !
 * @param
 * @param
 * @param
 * @return
 */
int8 P2PSvr_AddAllowInfo(void* const, int8*, AccessInfo*);

/**
 * NOT TO USE !
 * @param
 * @param
 * @param
 * @return
 */
int8 P2PSvr_RemoveAllowInfo(void* const, int8*, AccessInfo*);

///////////////////////////////////////////////////////////////////////////////
//																													//
//    释放     																								//
//																													//
///////////////////////////////////////////////////////////////////////////////

/**
 * P2PCtrl动态库释放
 */
void P2PCtrl_Free();

#endif /* UCP2PCTRL_H_ */
