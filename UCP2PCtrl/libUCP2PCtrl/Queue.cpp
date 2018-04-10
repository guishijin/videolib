/*
 * Queue.cpp 队列基类
 *
 *  特别注意，该队列的实现是允许数据丢失的。
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */

#include "Queue.h"

#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

/**
 * 构造函数(无参)
 */
CQueue::CQueue()
{
	//初始化临界区
	pthread_mutex_init(&section, NULL);
	pthread_mutex_lock(&section);

	m_queNum = QUEUENUM;
	m_isInit = false;
	m_queFreeSize = -1;

	m_queHead = 0;
	m_queTail = 0;
	m_maxDataLen = 0;
	m_maxLengthOFExtaraInfo = 0;

	pthread_mutex_unlock(&section);
}

/**
 * 析构函数
 */
CQueue::~CQueue()
{
	pthread_mutex_destroy(&section);
}

/**
 * 带参构造函数
 *
 *         调用此构造函数后可以不构造init函数生成每个元素具有_maxDataLen长
 *			度的队列，队列长度由宏QUENUM
 *
 * @param dataLen 所有元素中最长元素的长度
 * @param lengthOFExtaraInfo 所有元素中最长说明信息的长度
 */
CQueue::CQueue(int dataLen, int lengthOFExtaraInfo)
{
	//初始化临界区
	pthread_mutex_init(&section, NULL);

	pthread_mutex_lock(&section);
	m_isInit = true;
	m_queNum = QUEUENUM;

	m_maxDataLen = dataLen;
	m_maxLengthOFExtaraInfo = lengthOFExtaraInfo;
	m_queHead = 0;
	m_queTail = 0;

	//申请队列内存和初始化辅助信息指针
	for (int i = 0; i < m_queNum; i++)
	{
		m_queData[i].pVoidData = (unsigned char*) malloc(m_maxDataLen);
		if (m_queData[i].pVoidData == NULL)
		{
#ifdef _DEBUG
			printf("队列内存分配失败\n");
#endif
			m_isInit = false;
			break;
		}
		m_queData[i].pVoidExtraInfo = malloc(m_maxLengthOFExtaraInfo);
		if (m_queData[i].pVoidExtraInfo == NULL)
		{
#ifdef _DEBUG
			printf("队列内存分配失败\n");
#endif
			m_isInit = false;
			break;
		}
		m_queData[i].dataLen = m_maxDataLen;
		m_queData[i].lengthOFExtaraInfo = m_maxLengthOFExtaraInfo;
	}
	//m_queFreeSize是为了保存1个空余元素位置，同一适合head和tail不重合（同时读写）
	//获取getfreesize后，没有位置就直接不放
	m_queFreeSize = m_queNum - 1;

	pthread_mutex_unlock(&section);
}

/**
 * 队列初始化，配合无参构造函数使用
 *
 *				生成每个元素具有_maxDataLen长度的队列，队列长度由宏QUENUM
 *
 * @param dataLen 所有元素主数据中最长元素的长度
 * @param lengthOFExtaraInfo 所有元素数据最长说明信息的长度
 * @return true，成功；false，失败
 */
bool CQueue::queInit(int dataLen, int lengthOFExtaraInfo)
{
	//开始初始化
	pthread_mutex_lock(&section);

	//如果已经用构造函数构造或者调用过init函数，则返回true
	if (m_isInit)
	{
		pthread_mutex_unlock(&section);
		return true;
	}

	// 初始化基本参数
	m_maxDataLen = dataLen;
	m_maxLengthOFExtaraInfo = lengthOFExtaraInfo;
	m_queHead = 0;
	m_queTail = 0;

	// 清空队列数组
	memset(m_queData, 0, sizeof(m_queData));

	// 申请队列内存和初始化辅助信息指针
	for (int i = 0; i < m_queNum; i++)
	{
		// 分配队列数据元素空间
		m_queData[i].pVoidData = (unsigned char*) malloc(m_maxDataLen);
		if (m_queData[i].pVoidData == NULL)
		{
#ifdef _DEBUG
			printf("队列内存分配失败\n");
#endif

			pthread_mutex_unlock(&section);
			return false;
		}

		// 分配额外信息空间
		m_queData[i].pVoidExtraInfo = malloc(m_maxLengthOFExtaraInfo);
		if (m_queData[i].pVoidExtraInfo == NULL)
		{
#ifdef _DEBUG
			TRACE("队列内存分配失败\n");
#endif

			pthread_mutex_unlock(&section);
			return false;
		}

		// 设定空间长度
		m_queData[i].dataLen = m_maxDataLen;
		m_queData[i].lengthOFExtaraInfo = m_maxLengthOFExtaraInfo;
	}

	// 设置初始化完成标志
	m_isInit = true;

	// m_queFreeSize是为了保存1个空余元素位置，同一适合head和tail不重合（同时读写）
	// 获取getfreesize后，没有位置就直接不放
	m_queFreeSize = m_queNum - 1;

	pthread_mutex_unlock(&section);

	// 返回成功
	return true;
}

/**
 * 获取队列空闲的元素个数
 *
 * 		返回队列可用长度（元素个数）
 *
 * @return 大于0时，有空间；0，队列满
 */
int CQueue::GetFreeSize()
{
	return m_queFreeSize;
}

/**
 * 获取队列的空闲元素个数，同时，将队列的一个可用元素指针通过传出参数返回。
 *
 * @param stDataSrc 传出参数，空闲元素指针
 * @return 空闲元素个数， 大于0时，有空间；0，队列满
 */
int CQueue::GetFreeSizeEx(StQueueData **stDataSrc)
{
	pthread_mutex_lock(&section);

	if ((!m_isInit) || (m_queFreeSize < 1) || (stDataSrc == NULL))	//保留1个空间空闲
	{
		pthread_mutex_unlock(&section);
		return false;
	}

	//缓冲区长度赋值
	m_queData[m_queTail].dataLen = this->m_maxDataLen;
	m_queData[m_queTail].lengthOFExtaraInfo = this->m_maxLengthOFExtaraInfo;
	//返回尾指针对象
	*stDataSrc = &m_queData[m_queTail];

	pthread_mutex_unlock(&section);

	return m_queFreeSize;
}

/**
 * 向队列放入元素
 *
 * 			将data放入队列，放入前应调用GetFreeSize()获取队列长度
 *
 * @param stDataSrc 数据元素 ，放入的数据，不能为空
 * @return true, 有空间且放入成功；false, 队列满，放入失败
 */
bool CQueue::quePut(StQueueData *stDataSrc)
{
	pthread_mutex_lock(&section);

	if (!m_isInit || m_queFreeSize < 1 || stDataSrc == NULL)	//保留1个空间空闲
	{
		pthread_mutex_unlock(&section);
		return false;
	}

	//长度非法
	if (stDataSrc->dataLen > m_maxDataLen
			|| stDataSrc->lengthOFExtaraInfo > this->m_maxLengthOFExtaraInfo
			|| stDataSrc->dataLen < 0 || stDataSrc->lengthOFExtaraInfo < 0)
	{
		pthread_mutex_unlock(&section);
		return false;
	}

	//copy信息到队列
	memcpy(stDataSrc->pVoidData, m_queData[m_queTail].pVoidData,
			stDataSrc->dataLen);
	//copy额外信息到队列
	memcpy(stDataSrc->pVoidExtraInfo, m_queData[m_queTail].pVoidExtraInfo,
			stDataSrc->lengthOFExtaraInfo);
	m_queData[m_queTail].dataLen = stDataSrc->dataLen;
	m_queData[m_queTail].lengthOFExtaraInfo = stDataSrc->lengthOFExtaraInfo;

	//队尾添加数据，一直指向最后一个可用空间
	m_queTail++;
	m_queTail %= m_queNum;
	--m_queFreeSize;

	pthread_mutex_unlock(&section);

	return true;
}

/**
 * 获取元素扩展函数，将头部元素的指针通过传出参数返回。
 * @param stDataDst 头元素传出指针。
 * @return 主数据的长度
 */
int32 CQueue::queGetEx(StQueueData **stDataDst)
{
	pthread_mutex_lock(&section);

	// m_queNum - 1，保留1个空间，如果==m_queNUm-1，说明没有数据
	if (!m_isInit || m_queFreeSize >= m_queNum - 1)
	{
		pthread_mutex_unlock(&section);
		return -1;
	}
	if (NULL == m_queData[m_queHead].pVoidData
			|| NULL == m_queData[m_queHead].pVoidExtraInfo)
	{
		pthread_mutex_unlock(&section);
		return -1;
	}
	//将输出指针指向队列数据
	*stDataDst = &m_queData[m_queHead];

	m_queHead++;
	m_queHead %= m_queNum;
	++m_queFreeSize;

	pthread_mutex_unlock(&section);
	return ((*stDataDst)->dataLen);
}

/**
 * 放入元素扩展，放入元素的同时，将队列中的可用尾部空闲元素指针通过传出参数返回。
 *
 * 			将data放入队列，放入前应调用GetFreeSizeEx()获取队列长度及尾队列指针
 *
 * @param stDataSrc 返回新的可用空闲队尾数据指针。
 * @return true，有空间且放入成功；false，队列满，放入失败
 */
bool CQueue::quePutEx(StQueueData **stDataSrc)
{
	pthread_mutex_lock(&section);

	//保留1个空间空闲
	if (!m_isInit || m_queFreeSize < 1)
	{
		//放入失败,需初始化元素内存长度
		(*stDataSrc)->dataLen = this->m_maxDataLen;

		pthread_mutex_unlock(&section);
		return false;
	}

	//队尾添加数据，一直指向最后一个可用空间
	m_queTail++;
	m_queTail %= m_queNum;
	--m_queFreeSize;

	//缓冲区长度赋值
	m_queData[m_queTail].dataLen = this->m_maxDataLen;
	m_queData[m_queTail].lengthOFExtaraInfo = this->m_maxLengthOFExtaraInfo;
	//返回尾指针对象
	*stDataSrc = &m_queData[m_queTail];

	pthread_mutex_unlock(&section);

	return true;
}

/**
 * 获取队列中的元素
 * @param stDataDst [out]放置取出的数据
 * @return 主数据的长度
 */
int32 CQueue::queGet(StQueueData *stDataDst)
{
	pthread_mutex_lock(&section);

	//m_queNum - 1，保留1个空间，如果==m_queNUm-1，说明没有数据
	if (!m_isInit || m_queFreeSize >= m_queNum - 1)
	{
		pthread_mutex_unlock(&section);
		return -1;
	}

	if (NULL == m_queData[m_queHead].pVoidData
			|| NULL == m_queData[m_queHead].pVoidExtraInfo)
	{
		pthread_mutex_unlock(&section);
		return -1;
	}

	//将输出指针指向队列数据
	stDataDst->pVoidData = m_queData[m_queHead].pVoidData;
	stDataDst->pVoidExtraInfo = m_queData[m_queHead].pVoidExtraInfo;
	//长度校正
	stDataDst->dataLen = m_queData[m_queHead].dataLen;
	stDataDst->lengthOFExtaraInfo = m_queData[m_queHead].lengthOFExtaraInfo;

	m_queHead++;
	m_queHead %= m_queNum;
	++m_queFreeSize;

	pthread_mutex_unlock(&section);
	return stDataDst->dataLen;
}

/**
 * 复制队列数据(只复制数据，不复制说明信息。)
 * @param stData 【out】，复制出的数据
 * @return 主数据的长度
 */
int32 CQueue::queCopy(StQueueData *stData)
{
	pthread_mutex_lock(&section);

	//m_queNum - 1，保留1个空间，如果==m_queNUm-1，说明没有数据
	if (!m_isInit || m_queFreeSize >= m_queNum - 1)
	{
		pthread_mutex_unlock(&section);
		return -1;
	}

	if (NULL == m_queData[m_queHead].pVoidData
			|| NULL == m_queData[m_queHead].pVoidExtraInfo)
	{
		pthread_mutex_unlock(&section);
		return -1;
	}

	memcpy(stData->pVoidData, m_queData[m_queHead].pVoidData,
			m_queData[m_queHead].dataLen);
	//memcpy(stData->pVoidExtraInfo,m_queData[m_queHead].pVoidExtraInfo,m_queData[m_queHead].lengthOFExtaraInfo);
	//长度校正
	stData->dataLen = m_queData[m_queHead].dataLen;
	//stData->lengthOFExtaraInfo = m_queData[m_queHead].lengthOFExtaraInfo;

	pthread_mutex_unlock(&section);
	return stData->dataLen;
}

/**
 * 清空队列，但不释放队列空间
 */
void CQueue::queClear()
{
	pthread_mutex_lock(&section);

	m_queHead = 0;
	m_queTail = 0;
	m_queFreeSize = m_queNum - 1;	//保留一个空间

	pthread_mutex_unlock(&section);
}

/**
 * 释放队列内存
 */
void CQueue::queFree()
{
	pthread_mutex_lock(&section);

	m_isInit = false;
	for (int i = 0; i < m_queNum; i++)
	{
		if (NULL != m_queData[i].pVoidData)
			free(m_queData[i].pVoidData);
		m_queData[i].pVoidData = NULL;
		if (NULL != m_queData[i].pVoidExtraInfo)
			free(m_queData[i].pVoidExtraInfo);
		m_queData[i].pVoidExtraInfo = NULL;
		m_queData[i].dataLen = -1;
		m_queData[i].lengthOFExtaraInfo = -1;
	}
	m_queFreeSize = -1;
	m_queHead = 0;
	m_queTail = 0;

	pthread_mutex_unlock(&section);
}

/**
 * 获取队列大小
 * @return
 */
int32 CQueue::GetQueSize()
{
	return m_queData[m_queTail - 1].dataLen;
}

/**
 * 检查是否已经初始化
 * @return
 */
bool CQueue::whetherInit()
{
	return m_isInit;
}

/**
 * 拷贝的方式放入元素
 *
 * 		将data放入队列，放入前应调用GetFreeSize()获取队列长度
 *
 * @param stDataSrc  【in】，放入的数据，不能为空
 * @return  true，有空间且放入成功；0，队列满，放入失败
 */
bool CQueue::quePutA(StQueueData *stDataSrc)
{
	pthread_mutex_lock(&section);

	if (!m_isInit || m_queFreeSize < 1 || stDataSrc == NULL)	//保留1个空间空闲
	{
		pthread_mutex_unlock(&section);
		return false;
	}

	//长度非法
	if (stDataSrc->dataLen > m_maxDataLen
			|| stDataSrc->lengthOFExtaraInfo > this->m_maxLengthOFExtaraInfo
			|| stDataSrc->dataLen < 0 || stDataSrc->lengthOFExtaraInfo < 0)
	{
		pthread_mutex_unlock(&section);
		return false;
	}

	//copy信息到队列
	memcpy(m_queData[m_queTail].pVoidData, stDataSrc->pVoidData,
			stDataSrc->dataLen);
	//copy额外信息到队列
	memcpy(m_queData[m_queTail].pVoidExtraInfo, stDataSrc->pVoidExtraInfo,
			stDataSrc->lengthOFExtaraInfo);
	m_queData[m_queTail].dataLen = stDataSrc->dataLen;
	m_queData[m_queTail].lengthOFExtaraInfo = stDataSrc->lengthOFExtaraInfo;

	//队尾添加数据，一直指向最后一个可用空间
	m_queTail++;
	m_queTail %= m_queNum;
	--m_queFreeSize;

	pthread_mutex_unlock(&section);

	return true;
}

