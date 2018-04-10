/*
 * CMyQueue.cpp
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */

#include "CMyQueue.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//构造函数
CMyQueue::CMyQueue()
{
	//初始化标志
	m_bInitFlag = 0;
	//分配的内存缓冲区
	m_pMallocBuf = NULL;
	//分配的内存大小
	m_uMemSize = 0;
	//开始位置
	m_pStartPos = NULL;
	//结束位置
	m_pEndPos = NULL;
	//读取位置
	m_pReadBuf = NULL;
	//写入位置
	m_pWriteBuf = NULL;
	//对象结构的大小
	m_uStructSize = 0;
	//数据字段长度
	m_uMaxDataSize = 0;
	//扩展信息字段长度
	m_uMaxExtaraInfoSize = 0;
	//输出元素指针
	m_outQueueData = NULL;
	m_outQueueLen = 0;
	//输入元素指针
	m_inQueueData = NULL;
	m_inQueueLen = 0;
	//一个元素的大小
	m_uOneStDataSize = 0;
	//三个元素的大小
	m_uThreeStDataSize = 0;

	// 消除警告信息
	this->m_bPutResult = false;
}
//析构函数
CMyQueue::~CMyQueue()
{
	//释放分配内存
	this->queFree();
}
//初始化分配内存
int CMyQueue::queInit(unsigned int uMemSize, unsigned int uMaxDataSize, unsigned int uMaxExtaraInfoSize)
{
	//分配的内存缓冲区
	m_pMallocBuf = (unsigned char*)malloc(uMemSize);
	//判断内存是否分配成功
	if (m_pMallocBuf == NULL)
	{
		//内存分配失败，返回-1
		return -1;
	}
	//分配的内存大小
	m_uMemSize = uMemSize;
	//开始位置
	m_pStartPos = m_pMallocBuf;
	//结束位置
	m_pEndPos = m_pMallocBuf + uMemSize;
	//读取位置
	m_pReadBuf = m_pMallocBuf;
	//写入位置
	m_pWriteBuf = m_pMallocBuf;
	//对象结构的大小
	m_uStructSize = sizeof(StMyQueueData);
	//数据字段长度
	m_uMaxDataSize = uMaxDataSize;
	//扩展信息字段长度
	m_uMaxExtaraInfoSize = uMaxExtaraInfoSize;
	//一个元素的大小
	m_uOneStDataSize = m_uStructSize + uMaxDataSize + uMaxExtaraInfoSize + 4096;
	//两个元素的大小
	m_uThreeStDataSize = m_uOneStDataSize * 3;
	//初始化标志
	m_bInitFlag = 1;

	//返回成功
	return 0;
}
//释放分配内存
int CMyQueue::queFree()
{
	//判断是否已初始化
	if (m_bInitFlag != 1)
	{
		return 0;
	}
	//将初始化标志修改为0
	m_bInitFlag = 0;
	//释放内存
	free(m_pMallocBuf);
	m_pMallocBuf = 0;
	//分配的内存大小
	m_uMemSize = 0;
	//开始位置
	m_pStartPos = NULL;
	//结束位置
	m_pEndPos = NULL;
	//读取位置
	m_pReadBuf = NULL;
	//写入位置
	m_pWriteBuf = NULL;
	//对象结构的大小
	m_uStructSize = 0;
	//数据字段长度
	m_uMaxDataSize = 0;
	//扩展信息字段长度
	m_uMaxExtaraInfoSize = 0;
	//输出元素指针
	m_outQueueData = NULL;
	m_outQueueLen = 0;
	//输入元素指针
	m_inQueueData = NULL;
	m_inQueueLen = 0;
	//一个元素的大小
	m_uOneStDataSize = 0;
	//三个元素的大小
	m_uThreeStDataSize = 0;

	return 0;
}

//清空队列
int CMyQueue::queClear()
{
	//判断是否已初始化
	if (m_bInitFlag != 1)
	{
		return 0;
	}
	//读取位置
	m_pReadBuf = m_pMallocBuf;
	//写入位置
	m_pWriteBuf = m_pMallocBuf;

	//返回成功
	return 0;
}
//获取剩余空间
int CMyQueue::queGetFreeSize()
{
	//剩余空间
	int uFreeSize = (int)(m_pWriteBuf - m_pReadBuf);
	if (uFreeSize >= 0)
	{
		return (m_uMemSize - uFreeSize);
	}
	else
	{
		return abs(uFreeSize);
	}
}
int CMyQueue::queGetFreeSizeEx(StMyQueueData** stDataDst)
{
	//判断是否已初始化
	if (m_bInitFlag != 1)
	{
		//未初始化 返回-1
		return (-1);
	}
	//剩余空间
	int uFreeSize = (int)(m_pWriteBuf - m_pReadBuf);
	if (uFreeSize >= 0)
	{
		uFreeSize = (m_uMemSize - uFreeSize);
	}else
	{
		uFreeSize = abs(uFreeSize);
	}
	//如果剩余空间小于三个队列元素时，禁止添加新数据
	if ((unsigned int)uFreeSize < m_uThreeStDataSize)
	{
		//队列元素对象
		*stDataDst = m_inQueueData = NULL;
		//剩余空间
		return uFreeSize;
	}
	//队列元素对象
	*stDataDst = m_inQueueData = (StMyQueueData*)m_pWriteBuf;
	//如果对象不为空 则初始化对象
	if (m_inQueueData != NULL)
	{
		//重新初始化元素的长度
		m_inQueueData->dataLen = m_uMaxDataSize;
		m_inQueueData->lengthOFExtaraInfo = m_uMaxExtaraInfoSize;
		//初始化内存指针
		m_inQueueData->pVoidData = (void*)(m_inQueueData + m_uStructSize);
		m_inQueueData->pVoidExtraInfo = NULL;
	}
	//剩余空间
	return uFreeSize;
}
//将数据放入到队列
bool CMyQueue::quePut(StMyQueueData** stDataSrc)
{
	//判断是否已初始化
	if (m_bInitFlag != 1)
	{
		return (false);
	}
	//队列元素对象
	m_inQueueData = *stDataSrc;
	//剩余空间
	int uFreeSize = (int)(m_pWriteBuf - m_pReadBuf);
	if (uFreeSize >= 0)
	{
		uFreeSize = (m_uMemSize - uFreeSize);
	}
	else
	{
		uFreeSize = abs(uFreeSize);
	}
	//如果剩余空间小于三个队列元素时，禁止添加新数据
	if ((unsigned int)uFreeSize >= m_uThreeStDataSize)
	{
		//判断传入参数
		if ((m_inQueueData != NULL) && ((unsigned char*)m_inQueueData == m_pWriteBuf))
		{
			//对象占用内存长度
			m_inQueueLen = m_uStructSize + (m_inQueueData->dataLen + m_inQueueData->lengthOFExtaraInfo) + 4096;
			//移动指针
			m_pWriteBuf += m_inQueueLen;
			//如果顺序空间小于一个元素大小，跳转到头
			if ((unsigned int)(m_pEndPos - m_pWriteBuf) < m_uOneStDataSize)
			{
				//写指针跳转到队列头
				m_pWriteBuf = m_pStartPos;
			}
		}
		//队列元素对象
		*stDataSrc = m_inQueueData = (StMyQueueData*)m_pWriteBuf;
		//数据放入队列成功
		m_bPutResult = true;
	}
	else
	{
		//数据放入队列失败
		m_bPutResult = false;
	}
	//如果对象不为空 则初始化对象
	if (m_inQueueData != NULL)
	{
		//重新初始化元素的长度
		m_inQueueData->dataLen = m_uMaxDataSize;
		m_inQueueData->lengthOFExtaraInfo = m_uMaxExtaraInfoSize;
		//初始化内存指针
		m_inQueueData->pVoidData = (void*)(m_inQueueData + m_uStructSize);
	}

	//返回结果
	return m_bPutResult;
}
//获取队列中的数据
int CMyQueue::queGet(StMyQueueData** stDataDst, bool bFlag)
{
	//判断是否已初始化
	if (m_bInitFlag != 1)
	{
		return (-1);
	}
	//判断队列是否为空
	if (m_pWriteBuf == m_pReadBuf)
	{
		//队列指针
		*stDataDst = NULL;
		//返回数据长度
		return -1;
	}
	//给输出参数赋值
	*stDataDst = (StMyQueueData*)m_pReadBuf;
	//不移动读取指针
	if (bFlag == false)
	{
		return 0;
	}
	//获取队列元素对象
	m_outQueueData = (StMyQueueData*)m_pReadBuf;
	//元素占用内存大小
	m_outQueueLen = m_uStructSize + (m_outQueueData->dataLen + m_outQueueData->lengthOFExtaraInfo) + 4096;
	m_pReadBuf += m_outQueueLen;
	//判断剩余缓冲区是否足够放一个元素
	if ((unsigned int)(m_pEndPos - m_pReadBuf) < m_uOneStDataSize)
	{
		//读指针跳转到队列头
		m_pReadBuf = m_pStartPos;
	}
	return 0;
}

