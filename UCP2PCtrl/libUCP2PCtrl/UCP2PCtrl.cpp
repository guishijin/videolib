/*
 * UCP2PCtrl.cpp
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */

#include <stdio.h>
#include <unistd.h>

#include "typedef.h"
#include "CommUDP.h"
#include "CommTCP.h"

#include "CRtspServer.h"
#include "CRtspClient.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern char g_chHostName[128];		//主机名称
extern int g_nHostLen;				//主机名长度

//P2PCtrl动态库初始化
bool P2PCtrl_Init()
{
	UDP_Init();
	TCP_Init();

	//获取主机名称
	gethostname(g_chHostName,128);
	g_nHostLen = (int8)strlen(g_chHostName);

	return true;
}
/********************************************
函数接口: void*  P2PNode_Create(int8* chIP, uint32 nPort);
入口参数: chIP		服务IP地址；
		  nPort		服务器端口；
出口参数: 无
返 回 值: 0:失败	>0:客户端ID
函数功能: 创建一个客户端。
********************************************/
void* P2PNode_Create(char* chIP, uint32 const nPort,char* AppInfo,uint32  nType)
{
	//声明类指针
	CRtspClient* pClient = 0;
	//实例化类 根据AppInfo的值创建不同类型客户端 1-解码器 3-存储服务器 4-WEB
	pClient = new CRtspClient((char*)AppInfo,nType);
	if(pClient == NULL)
	{
		return 0;
	}
	//初始化
	if(pClient->Init((char*)chIP, (uint16)nPort) != 0){
		//删除节点
		delete pClient;
		//返回失败
		return 0;
	}
	//返回类地址 作为客户端ID
	return (void*)pClient;
}
/********************************************
函数接口: void*  P2PNode_Create(int8* chIP, uint32 nPort);
入口参数: chIP		服务IP地址；
		  nPort		服务器端口；
出口参数: 无
返 回 值: 0:失败	>0:客户端ID
函数功能: 创建一个客户端。
********************************************/
void* P2PNode_CreateEx(char* chIP, uint32 const nPort,char* AppInfo,uint32  nType,char* chUser,char* chPwd, uint8 bOverTCP)
{
	//声明类指针
	CRtspClient* pClient = 0;

	//实例化类 根据AppInfo的值创建不同类型客户端 1-解码器 3-存储服务器 4-WEB
	if(bOverTCP == 0)
	{
		pClient = new CRtspClient((char*)AppInfo, nType, NULL, (char*)chUser, (char*)chPwd, false);
	}
	else
	{
		pClient = new CRtspClient((char*)AppInfo, nType, NULL, (char*)chUser, (char*)chPwd, true);
	}
	if(pClient == NULL)
	{
		return 0;
	}
	//初始化
	if(pClient->Init((char*)chIP, (uint16)nPort) != 0)
	{
		//删除节点
		delete pClient;
		//返回失败
		return 0;
	}
	//返回类地址 作为客户端ID
	return (void*)pClient;
}
/********************************************
函数接口: int8  P2PNode_InitQueue(void* const pNodeID, uint32 uMemSize, uint32 uMaxDataSize, uint32 uMaxExtaraInfoSize);
入口参数: pNodeID			客户端ID；
		  uMemSize			队列内存大小；
		  uMaxDataSize		视频数据最大长度
		  uMaxExtaraInfoSize扩展信息最大长度
出口参数: 无
返 回 值: 0:成功	-1:失败
函数功能: 创建一个客户端。
********************************************/
int8 P2PNode_InitQueue(void* const pNodeID, uint32 uMemSize, uint32 uMaxDataSize, uint32 uMaxExtaraInfoSize)
{
	int8 result = -1;	//返回结果
	//声明类指针
	CRtspClient* pClient = 0;
	//判断合法性
	if(pNodeID == 0)return -1;
	//实例化类
	pClient = (CRtspClient*)pNodeID;

	//初始化视频队列
	result = pClient->InitQueue(uMemSize, uMaxDataSize, uMaxExtaraInfoSize);
	if(result==0)//成功发送
	{
		return 0;
	}
	else
	{
		return -1;
	}
}
/********************************************
请求视频资源下载地址
函数接口: int8 P2PNode_ GetStreamURL(void* pNodeID, int8* chName, int8* chVideoAddr);
入口参数: pNodeID	客户端ID；
 		  chName	资源名字；
出口参数: chVideoAddr	视频数据的获取地址；
返 回 值: 0:成功	-1:失败；
函数功能: 请求视频资源下载地址；
********************************************/
int8 P2PNode_GetStreamURL(void* const pNodeID,int8 *const chName, int8* chVideoAddr)
{
	int8 result = -1;	//返回结果
	//声明类指针
	CRtspClient* pClient = 0;
	//判断合法性
	if(pNodeID == 0)return -1;
	//实例化类
	pClient = (CRtspClient*)pNodeID;

	//根据 视频访问地址chAddr和资源名称chName 请求资源
	result = pClient->GetStreamURL(chName,chVideoAddr);
	if(result==0)//成功发送
	{
		return 0;
	}
	else
	{
		return -1;
	}
}
/********************************************
函数接口：int8 P2PNode_Start(void* pNodeID,
						int8* chName, int8* chAddr,
						CQueue* fQueue);
入口参数：	pNodeID		客户端ID；
			chAddr		视频访问地址；
 			chName		资源名字；
 			fQueue		缓存队列指针
出口参数：	无
返 回 值：	0：成功 -1：失败
函数功能：	开始下载视频数据
********************************************/
int8 P2PNode_Start(void* const pNodeID,int8 *const chName,int8 *const chAddr, CQueue* fQueue)
{
	int8 result = -1;	//返回结果
	//声明类指针
	CRtspClient* pClient = 0;
	//判断合法性
	if(pNodeID == 0)return -1;
	//实例化类
	pClient = (CRtspClient*)pNodeID;
	//获得可用操作命令
	if(pClient->Init(chName, chAddr, fQueue) != 0)
		return -1;
	//开始下载视频数据
	result = pClient->StartRun();
	if(result==0)
	{
		return 0;//成功发送
	}
	else
	{
		return -1;//发送失败
	}
}
/********************************************
得到节点状态
函数接口: int8 P2PNode_ GetStatus(void* pNodeID);
入口参数: pNodeID	客户端ID；
 		  chName	资源名字；
出口参数: chVideoAddr	视频数据的获取地址；
返 回 值: -1失败, 0:正在停止,1正在启动，2运行,	3停止
函数功能: 获取当前节点状态
********************************************/
int8 P2PNode_GetStatus(void* const pNodeID,double *nRecvPack,double *nRecvLen)
{
	int8 result = -1;	//返回结果
	//判断合法性
	if(pNodeID == 0)return -1;
	//声明类指针
	CRtspClient* pClient = 0;
	//实例化类
	pClient = (CRtspClient*)pNodeID;
	//得到数据长度
	pClient->GetDataCheck(nRecvPack,nRecvLen);
	//得到节点的视频状态
	result = pClient->GetVideoStatus();
	return result;
}
/********************************************
函数接口: int8  P2PNode_GetVideoData(void* const pNodeID, StQueueData* stVideoData)
入口参数: pNodeID			客户端ID；
		  stVideoData		视频数据结构
出口参数: 无
返 回 值: >0:成功	<=:失败
函数功能: 获取视频数据。
********************************************/
int32  P2PNode_GetVideoData(void* const pNodeID, StQueueData* stVideoData)
{
	int32 result = -1;	//返回结果
	//判断合法性
	if(pNodeID == 0)return -1;
	//客户端类指针
	CRtspClient* pClient = (CRtspClient*)pNodeID;

	//初始化视频队列
	result = pClient->GetVideoData(stVideoData);
	if(result > 0)//成功
	{
		return result;
	}
	else
	{
		return -1;
	}
}
/********************************************
关闭指定视频流下载
函数接口：	int8 P2PNode_Stop(void* pNodeID, int8* chName);
入口参数：	pNodeID	客户端ID；
			chAddr	视频访问地址；
			chName	资源名字；
出口参数：	无；
返 回 值：	0：成功 -1：失败
函数功能：	关闭指定视频流
********************************************/
int8 P2PNode_Stop(void* const pNodeID, int8 *const chName)
{
	//声明类指针
	CRtspClient* pClient = 0;
	//判断合法性
	if(pNodeID == 0)
	{
		return -1;
	}
	//实例化类
	pClient = (CRtspClient*)pNodeID;
	//停止视频数据下载，断开连接
	if(pClient->StopRun() == 0)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

/********************************************
销毁一个客户端实例
函数接口：	int8 P2PNode_Free(void* pNodeID);
入口参数：	pNodeID		客户端ID；
出口参数：	无
返 回 值：  0：成功 -1：失败
函数功能：	销毁一个客户端实例
********************************************/
int8 P2PNode_Free(void* const pNodeID)
{
	//声明类指针
	CRtspClient* pClient = 0;
	//判断合法性
	if(pNodeID == 0)
	{
		return -1;
	}
	//实例化类
	pClient = (CRtspClient*)pNodeID;
	//销毁
	delete pClient;
	return 0;
}

//////////////////////
//					//
//    服务器端		//
//					//
//////////////////////
///********************************************
//创建一个P2P服务模块
//函数接口：	void* P2PSvr_Create(int8* chServerIP, uint16 nServerPort ,CVideoListCtrl ** pList);
//入口参数：	chServerIP	服务器绑定IP地址;
//				nServerPort	服务器绑定端口号;
//				nThreadID	服务处理线程ID
//				nMsgID		通知消息编号
//出口参数：	pList	资源管理链表。
//返 回 值：	0：失败,	>0：成功,服务模块实例ID
//函数功能：	创建一个服务模块
//********************************************/
void* P2PSvr_Create(int8* chServerIP, uint16 nServerPort ,DWORD nThreadID, DWORD nMsgID)
{
	//声明类指针
	CRtspServer* pServer = 0;
	//实例化类
	pServer = new CRtspServer(chServerIP, nServerPort, nThreadID,nMsgID);
	if(pServer == NULL)
	{
		return (void*)NULL;
	}
	pServer->m_nMsgThreadID = nThreadID;
	pServer->m_nMsgID = nMsgID;
	//返回类地址 作为服务器端ID
	return (void*)pServer;
}
///********************************************
//启动服务
//函数接口：	int8 P2PSvr_Start(void* pSrvID,int8* AppInfo = NULL);
//入口参数：	pSrvID 服务模块实例ID; AppInfo	应用程序名称
//出口参数：	无
//返 回 值：	0：成功,1：失败
//函数功能：	启动一个服务模块
//********************************************/
int8 P2PSvr_Start(void* const pSrvID,int8* appInfo)
{
	//判断传入参数是否合法
	if(pSrvID == NULL)
	{
		return 1;
	}
	//声明类指针
	CRtspServer* pServer = 0;
	//实例化类
	pServer = (CRtspServer*)pSrvID;
	//启动服务模块
	return pServer->StartRun();
}

///********************************************
//添加修改视频流
//函数接口：	int8  P2PSvr_AddVideo(void* pSrvID, int8* chName，CVideoInfo* pInfo);
//入口参数：	pSrvID		服务模块实例ID;
//				chName		流名称
//				pInfo		视频流信息
//出口参数：	无;
//返 回 值：	0：成功,1：失败;
//函数功能：	在服务中添加一个视频流
//********************************************/
int8  P2PSvr_AddVideo(void* const pSrvID, int8* chName,CVideoInfo* pInfo)
{
	//判断传入参数是否合法
	if(pSrvID == NULL)
	{
		return 1;
	}
	//声明服务器模块类指针
	CRtspServer* pServer = 0;
	//实例化服务器
	pServer = (CRtspServer*)pSrvID;
	//在服务中添加一个视频流
	return pServer->SetStream(chName, pInfo);
}

///********************************************
//删除视频流
//函数接口：	int8  P2PSvr_RemoveVideo(void* pSrvID,int8* chName);
//入口参数：	pSrvID		服务模块实例ID
//				chName		流名称
//出口参数：	无
//返 回 值：	0：成功,1：失败
//函数功能：	从服务中删除一个流
//********************************************/
int8  P2PSvr_RemoveVideo(void* const pSrvID,int8* chName)
{
	//判断传入参数是否合法
	if(pSrvID == NULL)
	{
		return 1;
	}
	//声明服务器模块类指针
	CRtspServer* pServer = 0;
	//实例化服务器
	pServer = (CRtspServer*)pSrvID;
	//从服务中删除一个流
	return	pServer->RemoveStream(chName);
}
///********************************************
//启动指定的视频流
//函数接口：	int8  P2PSvr_StartVideo(void* pSrvID，int8* chName);
//入口参数：	pSrvID		服务模块实例ID；
//				chName		流名称；
//出口参数：	无；
//返 回 值：	0：成功，1：失败；
//函数功能：	启动指定的视频流，名称未指定时启动所有视频流。
//********************************************/
int8 P2PSvr_StartVideo(void* const pSrvID, int8* chName)
{
	//判断传入参数是否合法
	if(pSrvID == NULL)
	{
		return 1;
	}
	//声明服务器模块类指针
	CRtspServer* pServer = 0;
	//实例化服务器
	pServer = (CRtspServer*)pSrvID;

	//启动指定的视频流
	return pServer->StartVideo(chName);
}
///********************************************
//停止视频流服务
//函数接口：	int8  P2PSvr_StopVideo(void* pSrvID,int8* chName);
//入口参数：	pSrvID		服务模块实例ID；
//				chName		流名称；
//出口参数：	无；
//返 回 值：	0：成功，1：失败；
//函数功能：	停止指定视频流服务，名称未指定时，停止所有视频流服务。
//********************************************/
int8 P2PSvr_StopVideo(void* const pSrvID, int8* chName)
{
	//判断传入参数是否合法
	if(pSrvID == NULL)
	{
		return 1;
	}
	//声明服务器模块类指针
	CRtspServer* pServer = 0;
	//实例化服务器
	pServer = (CRtspServer*)pSrvID;

	//停止模块服务
	return pServer->StopVideo(chName);
}
///********************************************
//移除客户端节点
//函数接口：	int8  P2PSvr_RemoveNode(void* pSrvID, int8* chName,int8* chNodeIP, uint16 nNodePort)
//入口参数：	pSrvID		服务模块实例ID；
//				chName		流名称；
//				chNodeIP	客户端IP
//				nNodePort	客户端端口
//出口参数：	无；
//返 回 值：	0：成功，1：失败；
//函数功能：	停止指定视频流服务，名称未指定时，停止所有视频流服务。
//********************************************/
int8 P2PSvr_RemoveNode(void* const pSrvID, int8* chName,int8* chNodeIP, uint16 nNodePort)
{
	//判断传入参数是否合法
	if(pSrvID == NULL)
	{
		return 1;
	}
	//声明服务器模块类指针
	CRtspServer* pServer = 0;
	//实例化服务器
	pServer = (CRtspServer*)pSrvID;

	//移除客户端节点
	return pServer->RemoveRtspClient(chName, chNodeIP, nNodePort);
}
///********************************************
//停止服务
//函数接口：	int8 P2PSvr_Stop(void* pSrvID);
//入口参数：	pSrvID	服务模块实例ID
//出口参数：	无
//返 回 值：	0：成功,1：失败
//函数功能：	停止模块服务
//********************************************/
int8 P2PSvr_Stop(void* const pSrvID)
{
	//判断传入参数是否合法
	if(pSrvID == NULL)
	{
		return 1;
	}
	//声明服务器模块类指针
	CRtspServer* pServer = 0;
	//实例化服务器
	pServer = (CRtspServer*)pSrvID;
	//停止模块服务
	pServer->StopRun();
	return 0;
}
///********************************************
//销毁一个P2P服务模块
//函数接口：	int32 P2PSvr_Destroy(int32 SrvID);
//入口参数：	SrvID	服务模块实例ID;
//出口参数：	无;
//返 回 值：	0：成功,1：失败;
//函数功能：	停止指定模块服务器,释放资源并销毁服务模块
//********************************************/
int32 P2PSvr_Destroy(void* const pSrvID)
{
	//判断传入参数是否合法
	if(pSrvID == NULL)
	{
		return 1;
	}
	//声明服务器模块类指针
	CRtspServer* pServer = 0;
	//实例化服务器
	pServer = (CRtspServer*)pSrvID;
	//销毁
	delete pServer;
	return 0;
}

///********************************************
//添加允许拒绝信息
//函数接口：	int8  P2PSvr_AddAllowInfo(void* pSrvID, int8* chName,AccessInfo * pAccessInfo)
//入口参数：	pSrvID		服务模块实例ID；
//				chName		流名称；
//				pAccessInfo	允许拒绝列表
//出口参数：	无；
//返 回 值：	0：成功，1：失败；
//函数功能：	向某资源中添加允许拒绝列表。
//********************************************/
int8 P2PSvr_AddAllowInfo(void* const , int8* ,AccessInfo* )
{
	return 0;
}

///********************************************
//删除允许拒绝信息
//函数接口：	int8  P2PSvr_RemoveAllowInfo(void* pSrvID, int8* chName,AccessInfo * pAccessInfo)
//入口参数：	pSrvID		服务模块实例ID；
//				chName		流名称；
//				pAccessInfo	允许拒绝列表
//出口参数：	无；
//返 回 值：	0：成功，1：失败；
//函数功能：	向某资源中删除允许拒绝列表。
//********************************************/
int8 P2PSvr_RemoveAllowInfo(void* const , int8* ,AccessInfo* )
{
	return 0;
}

//P2PCtrl动态库初始化
void P2PCtrl_Free()
{
	UDP_Free();
	TCP_Free();
}
