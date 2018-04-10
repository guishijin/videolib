/*
 * Queue.h
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */

#ifndef QUEUE_H_
#define QUEUE_H_

// 引用类型定义文件
#include "typedef.h"
#include "pthread.h"

// 队列长度
#define QUEUENUM	25

/**
 * 队列结构
 */
typedef struct
{
	void *pVoidData;					//指向队列元素，是一个dataLen长度的内存块
	void *pVoidExtraInfo;			//指向队列元素辅助说明信息，是一个lengthOFExtaraInfo长的内存块
	int32 dataLen;						//元素的长度
	int32 lengthOFExtaraInfo;	//元素辅助信息的长度
} StQueueData;

/**
 * 队列类
 */
class CQueue
{
public:
	/**
	 * 构造函数
	 */
	CQueue(void);

	/**
	 * 带参构造函数
	 * @param dataLen
	 * @param lengthOFExtaraInfo
	 */
	CQueue(int dataLen, int lengthOFExtaraInfo);

	/**
	 * 析构函数
	 */
	virtual ~CQueue(void);

	/**
	 * 队列初始化函数，配合无参构造函数使用
	 * @param dataLen
	 * @param lengthOFExtaraInfo
	 * @return
	 */
	bool queInit(int dataLen, int lengthOFExtaraInfo);		//完成队列中内存的申请

	/**
	 * 放入数据
	 * @param stData
	 * @return
	 */
	bool quePut(StQueueData *stData);				//放入数据

	/**
	 * 取出数据
	 * @param stData
	 * @return
	 */
	int32 queGet(StQueueData *stData);				//取出数据,如果返回0，没有可用数据

	/**
	 * 放入数据，扩展函数，提高效率，和queuGetEx配合使用
	 * @param stDataSrc
	 * @return
	 */
	bool quePutEx(StQueueData **stDataSrc);			//放入数据

	/**
	 * 取出数据
	 * @param stDataDst
	 * @return
	 */
	int32 queGetEx(StQueueData **stDataDst);			//取出数据,如果返回0，没有可用数据

	/**
	 * 复制队列中的数据
	 * @param stData
	 * @return
	 */
	int32 queCopy(StQueueData *stData);               //复制队列中的数据，如果返回0，没有可用数据

	/**
	 * 释放内存
	 */
	void queFree();									//释放内存。

	/**
	 * 获取队列中可用空间个数
	 * @return
	 */
	int GetFreeSize();								//获取队列可用空间个数

	/**
	 * 获取队列中可用空间个数，扩展函数，同时返回队尾指针。
	 * @param stData 队尾指针
	 * @return 空间个数
	 */
	int GetFreeSizeEx(StQueueData **stData);		//获取队列可用空间个数

	/**
	 * 清空队列，但是保存内存空间。
	 */
	void queClear();									//清空队列，但是保留内存空间

	/**
	 * 获取队列最大容量大小
	 * @return
	 */
	int32 GetQueSize();

	/**
	 * 是否初始化
	 * @return
	 */
	bool whetherInit();

	/**
	 * 放入数据扩展
	 * @param stData
	 * @return
	 */
	bool quePutA(StQueueData *stData);				//放入数据

protected:
	//***************************队列内部数据**begin*********************
	int m_queNum;				//队列长度，在构造时确定
	int m_maxDataLen;			//队列中每个数据的长度
	int m_maxLengthOFExtaraInfo;			//辅助信息的长度
	int m_queHead;				//队列头
	int m_queTail;					//队列尾
	int m_queFreeSize;			//剩余可用空间大小
	bool m_isInit;					//此队列是否初始化

	//队列临界区，使用pthread实现
	pthread_mutex_t section;
	//***************************队列内部数据**end***********************

	// 队列数组
	StQueueData m_queData[QUEUENUM];	//队列数据
};

#endif /* QUEUE_H_ */
