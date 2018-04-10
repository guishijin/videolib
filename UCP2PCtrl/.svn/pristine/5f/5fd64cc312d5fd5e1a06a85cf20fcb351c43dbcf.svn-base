/*
 * CVideoInfo.h
 *
 *  Created on: 2017年8月9日
 *      Author: root
 */

#ifndef CVIDEOINFO_H_
#define CVIDEOINFO_H_

#include "typedef.h"

/**
 * 资源信息结构体
 */
typedef struct
{
	int8 chKey[32];		//资源的键值，唯一性
	int32 nVideoID;		//资源ID编号
	int8 chVideoSrc[256];		//资源源地址
	int8 chVideoSrv[256];		//资源服务地址
	int32 nVideoStatus;	//资源状态
	int8 nYTType[32];	//云台类型编号
	int8 chEncFormat[32];	//视频编码格式
	int8 chSession[32];	//播放时的Session信息
	int8 chTrack[32];	//播放时的track信息
	int8 chUser[32];		//访问用户名
	int8 chPwd[32];		//访问密码
	int32 nVideoWidth;	//视频画面宽度
	int32 nVideoHeight;	//视频画面高度
	int32 nVideoFrames;	//视频帧速
	int32 nVideoBps;		//视频比特率
	int32 nStartPort;		//记录节点起始端口
	int8 bOverTCP;		//是否使用TCP传输视频数据
	void* pRtspClient;	//与编码器的连接套接字
} CVideoInfo;



#endif /* CVIDEOINFO_H_ */
