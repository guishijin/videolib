/*
 ============================================================================
 Name        : UCP2PCtrlTest.cpp
 Author      : gsj
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C++,
 ============================================================================
 */

#include <iostream>

#include <typedef.h>
#include <UCP2PCtrl.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

using namespace std;

int main(void) {
	cout << "Hello World" << endl; /* prints Hello World */


	// 关闭 SIGPIPE信号
	signal(SIGPIPE, SIG_IGN);

	// 忽略线程内部的SIGPIPE信号
	sigset_t signal_mask;
	sigemptyset (&signal_mask);
	sigaddset (&signal_mask, SIGPIPE);
	int rc = pthread_sigmask (SIG_BLOCK, &signal_mask, NULL);
	if (rc != 0)
	{
	   printf("block sigpipe error\n");
	}

	P2PCtrl_Init();

	char* ip = "172.16.10.10";
	void * pserver = P2PSvr_Create(ip,554,0,0);
	if(pserver != NULL)
	{
		char* appinfo = "test-Program";
		int ret = P2PSvr_Start(pserver,appinfo);
		if(ret == 0)
		{
			CVideoInfo cvi;
			sprintf((char*)cvi.chKey,"0001");
			cvi.nVideoID = 1;
			sprintf((char*)cvi.chVideoSrc,"rtsp://172.16.10.245:554/4128");
			sprintf((char*)cvi.chVideoSrv,"rtsp://172.16.10.10:554/0001");
			sprintf((char*)cvi.chEncFormat,"H264");

			char* streaminfo= "0001";
			P2PSvr_AddVideo(pserver,streaminfo,&cvi);
			P2PSvr_StartVideo(pserver,streaminfo);
		}

	}


	while(true)
	{
		usleep(1000*1000);
		printf(".");
	}


	return 0;
}
