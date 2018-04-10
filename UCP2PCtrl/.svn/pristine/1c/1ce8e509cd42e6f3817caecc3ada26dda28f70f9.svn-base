/*
 * CMyQueue.h
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */

#ifndef CMYQUEUE_H_
#define CMYQUEUE_H_

//队列元素
typedef struct{
	void			*pVoidData;				//指向队列元素，是一个dataLen长度的内存块
	void			*pVoidExtraInfo;		//指向队列元素辅助说明信息，是一个lengthOFExtaraInfo长的内存块
	long			dataLen;				//元素的长度
	long			lengthOFExtaraInfo;		//元素辅助信息的长度
}StMyQueueData;

//队列类
class CMyQueue
{
public:
	//构造函数
	CMyQueue();
	//析构函数
	~CMyQueue();
public:
	//初始化分配内存
	int queInit(unsigned int uMemSize, unsigned int uMaxDataSize, unsigned int uMaxExtaraInfoSize);
	//释放分配内存
	int queFree();
	//清空队列
	int queClear();
	//获取剩余空间
	int queGetFreeSize();
	int queGetFreeSizeEx(StMyQueueData** pDataBuf);
	//将数据放入到队列
	bool quePut(StMyQueueData** pDataBuf);
	//获取队列中的数据
	int queGet(StMyQueueData** pDataBuf, bool bFlag);
private:
	//初始化标志
	unsigned char m_bInitFlag;
	//分配的内存缓冲区
	unsigned char* m_pMallocBuf;
	//分配的内存大小
	unsigned int   m_uMemSize;
	//开始位置
	unsigned char* m_pStartPos;
	//结束位置
	unsigned char* m_pEndPos;
	//读取位置
	unsigned char* m_pReadBuf;
	//写入位置
	unsigned char* m_pWriteBuf;
private:
	//对象结构的大小
	unsigned int m_uStructSize;
	//数据字段长度
	unsigned int m_uMaxDataSize;
	//扩展信息字段长度
	unsigned int m_uMaxExtaraInfoSize;
	//输出元素指针
	StMyQueueData* m_outQueueData;
	unsigned int   m_outQueueLen;
	bool		   m_bPutResult;
	//输入元素指针
	StMyQueueData* m_inQueueData;
	unsigned int   m_inQueueLen;
	//一个元素的大小
	unsigned int   m_uOneStDataSize;
	//三个元素的大小
	unsigned int   m_uThreeStDataSize;
};



#endif /* CMYQUEUE_H_ */
