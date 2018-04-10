/*
 ============================================================================
 Name        : exampleProgram.c
 Author      : gsj
 Version     :
 Copyright   : Your copyright notice
 Description : Uses shared library to print greeting
               To run the resulting executable the LD_LIBRARY_PATH must be
               set to ${project_loc}/libUCP2PCtrl/.libs
               Alternatively, libtool creates a wrapper shell script in the
               build directory of this program which can be used to run it.
               Here the script will be called exampleProgram.
 ============================================================================
 */


#include <iostream>

#include <typedef.h>
#include <UCP2PCtrl.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

using namespace std;

/**
 * 测试入口
 * @return
 */
int main(void) {
	cout << "Hello World" << endl; /* prints Hello World */

	P2PCtrl_Init();

	char* ip = "172.16.10.10";
	void * pserver = P2PSvr_Create(ip,554,0,0);
	if(pserver != NULL)
	{
		// 先添加视频资源，后启动服务器
		CVideoInfo cvi = {0x00};
		sprintf((char*)cvi.chKey,"0001");
		cvi.nVideoID = 1;
		sprintf((char*)cvi.chVideoSrc,"rtsp://172.16.10.245:554/4128");
		sprintf((char*)cvi.chVideoSrv,"rtsp://172.16.10.10:554/0001");
		sprintf((char*)cvi.chEncFormat,"H264");

		char* streaminfo= "0001";
		P2PSvr_AddVideo(pserver,(int8*)streaminfo,(CVideoInfo*)&cvi);
		P2PSvr_StartVideo(pserver,streaminfo);

		char* appinfo = "test-Program";
		int ret = P2PSvr_Start(pserver,appinfo);
		if(ret == 0)
		{
			printf("服务器启动成功!\r\n");
		}
		else
		{
			printf("服务器启动失败!\r\n");
		}
	}

	while(true)
	{
		usleep(1000*1000);
		printf(".\n");
	}

	return 0;
}
