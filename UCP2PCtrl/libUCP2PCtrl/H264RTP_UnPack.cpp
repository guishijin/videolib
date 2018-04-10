/*
 * H264RTP_UnPack.cpp
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */




#include <stdio.h>
#include "H264RTP_UnPack.h"

#include <netinet/in.h>
#include <string.h>

//构造函数
CH264_Rtp_UnPack::CH264_Rtp_UnPack(void* hr, unsigned char H264PayLoadType)
	:m_bSPSFound(false)
	,m_wSeq(1234)
	,m_ssrc(0)
	,seqNumCycle(0)
{
	//分配缓冲区
	m_pBuf = NULL;
	m_H264PayLoadType = H264PayLoadType ;
	m_pEnd = 0;
	m_pStart = 0;
	m_dwSize = 0 ;
	m_loastPack = 0;
	m_bLostFlag = 1;
	//*hr = 0 ;

	// 消除警告信息
	this->m_bBeginsFrame = 0;
	this->m_nFrameType = 0;
	this->PayloadSize = 0;
	this->pPayload = NULL;
	this->m_nNALUnitType = 0;
	this->m_bCompletesFrame = 0;

}
//析构函数
CH264_Rtp_UnPack::~CH264_Rtp_UnPack(void)
{
	m_pBuf = NULL;
}

//析构函数
void CH264_Rtp_UnPack::SetBuffer(unsigned char *pBuf, int nSize)
{
	//设置缓冲区地址
	m_pBuf = pBuf;
	//缓冲区开始和接收位置
	m_pEnd = pBuf + nSize ;
	m_pStart = pBuf + 9;
}

//pBuf为H264 RTP视频数据包，nSize为RTP视频数据包字节长度，outSize为输出视频数据帧字节长度
//返回值为指向视频数据帧的指针。输入数据可能被破坏
unsigned char * CH264_Rtp_UnPack::Parse_RTP_Packet (
		unsigned char  *pBuf,
		unsigned short nSize,
		int *outSize ,
		int *nType,
		unsigned int *nSeq,
		unsigned int *nSSRC,
		unsigned long *nTs,
		unsigned char* nLostFlag )
{
	//小于等于12 返回失败
	if ( nSize <= 12 )
	{
		return NULL ;
	}
	//特殊头信息长度
	unsigned char numBytesToSkip = 0;
	//解析rtp头信息
	unsigned char *cp = (unsigned char*)&m_RTP_Header ;
	cp[0] = pBuf[0] ;
	cp[1] = pBuf[1] ;
	m_RTP_Header.seq = ((pBuf[2]<<8)|pBuf[3]);
	m_RTP_Header.ts  = ((pBuf[4]<<24) | (pBuf[5]<<16) | (pBuf[6]<<8)| pBuf[7]);
	m_RTP_Header.ssrc= ((pBuf[8]<<24) | (pBuf[9]<<16) | (pBuf[10]<<8)| pBuf[11]);

	// 检查rtp版本 必须是2
	if ( m_RTP_Header.v != RTP_VERSION )
	{
		return NULL;
	}
	// 检查Payload类型是否匹配
	if ( m_RTP_Header.pt != m_H264PayLoadType)
	{
		return NULL ;
	}
	//处理rtp序号信息
	if(m_RTP_Header.seq < m_wSeq)
	{
		seqNumCycle += 0x10000;
	}
	//给输出序号赋值
	*nSeq = ntohl((unsigned long)m_RTP_Header.seq + seqNumCycle);
	//给输出的ssrc赋值
	*nSSRC= ntohl(m_RTP_Header.ssrc);

	//
	pPayload = pBuf + 12 ;
	PayloadSize = nSize - 12 ;
	//跳过同步源标识
	if( m_RTP_Header.cc )
	{
		unsigned int cclen = (m_RTP_Header.cc*4);
		if( PayloadSize < cclen )
		{
			return NULL;
		}
		PayloadSize -= cclen;
		pPayload	+= cclen;
	}
	//跳过头扩展信息
	if( m_RTP_Header.x )
	{
		if( PayloadSize < 4)
		{
			return NULL;
		}
		PayloadSize -= 4;
		pPayload    += 2;
		unsigned int extlen = ((pPayload[0]<<8)|pPayload[1]);
		extlen      *= 4;
		pPayload	+= 2;
		if ( PayloadSize < extlen)
		{
			return NULL;
		}
		PayloadSize -= extlen;
		pPayload    += extlen;
	}
	//处理填充字节
	if(m_RTP_Header.p)
	{
		if( PayloadSize <= 0)
		{
			return NULL;
		}
		unsigned int padlen = pPayload[PayloadSize-1];
		if(PayloadSize < padlen)
		{
			return NULL;
		}
		PayloadSize -= padlen;
	}
	//判断长度
	if(PayloadSize < 1)
	{
		return NULL;
	}
	//获取NALUnit类型
	m_nFrameType = m_nNALUnitType = pPayload[0] & 0x1f;
	switch(m_nNALUnitType)
	{
	case 24: // STAP-A
		numBytesToSkip = 1;
		break;
	case 25:
	case 26:
	case 27: // STAP-B, MTAP16, or MTAP24
		numBytesToSkip = 3;
		break;
	case 28:
	case 29: // FU-A or FU-B
		if(PayloadSize < 2)
		{
			return NULL;
		}
		m_bBeginsFrame = pPayload[1]&0x80;
		m_bCompletesFrame = pPayload[1]&0x40;
		if (m_bBeginsFrame)
		{
			pPayload[1] = (pPayload[0]&0xE0)|(pPayload[1]&0x1F);
			numBytesToSkip = 1;
		}
		else
		{ // The start bit is not set, so we skip both the FU indicator and header:
			numBytesToSkip = 2;
		}
		m_nFrameType = pPayload[1] & 0x1f;
		break;
	default: // This packet contains one complete NAL unit:
		m_bBeginsFrame = m_bCompletesFrame = 1;
		numBytesToSkip = 0;
		break;
	}
	//ssrc不同表示为第一个数据包
	if ( m_ssrc != m_RTP_Header.ssrc )
	{
		m_ssrc = m_RTP_Header.ssrc ;
		SetLostPacket();
	}
	//开始标志
	if ( m_bBeginsFrame )
	{
		//序号不连续 丢包处理
		if(m_RTP_Header.seq != m_wSeq + 1)
			SetLostPacket();
		//开始标志时 更新序号
		m_wSeq = m_RTP_Header.seq - 1;
		m_bSPSFound = true ;
	}
	if ( !m_bSPSFound )
	{
		return NULL ;
	}
	//判断包序号是否连续
	if ( m_RTP_Header.seq != (unsigned short)( m_wSeq + 1 ) )
	{
		m_wSeq = m_RTP_Header.seq ;
		SetLostPacket () ;
		return NULL ;
	}
	else
	{
		// 码流正常
		m_wSeq = m_RTP_Header.seq ;
		// 判断长度
		if(PayloadSize <= numBytesToSkip)
		{
			return NULL;
		}
		//帧开始 添加帧开始标志
		if (m_bBeginsFrame)
		{
			*((unsigned int*)(m_pStart)) = 0x01000000 ;
			m_pStart += 4 ;
			m_dwSize += 4 ;
		}
		//跳过特定头
		pPayload += numBytesToSkip ;
		PayloadSize -= numBytesToSkip ;
		//拷贝数据
		if ( m_pStart + PayloadSize < m_pEnd )
		{
			//CopyMemory (m_pStart, pPayload, PayloadSize ) ;
			memcpy(m_pStart,pPayload,PayloadSize);

			m_dwSize += PayloadSize ;
			m_pStart += PayloadSize ;
		}
		else //memory overflow
		{
			SetLostPacket () ;
			return NULL ;
		}
		//帧结束 给输出参数赋值
		if (m_bCompletesFrame)
		{
			//06 07 08为信息帧，信息帧不输出，与后面的I帧一起输出
			if((m_nFrameType == 0x07 || m_nFrameType == 0x08 || m_nFrameType == 0x06) && (m_pStart - m_pBuf < 1024))
				return NULL;
			//输出有效数据长度
			*outSize = (int)(m_pStart - m_pBuf) ;
			//丢包标志
			*nLostFlag = m_bLostFlag;
			m_bLostFlag = 0;
			//访问单元分隔符
			if((m_nFrameType == 0x09) && (*outSize > 20))
			{
				//获取帧类型
				if(*(int*)(m_pBuf+15) == 0x01000000)
					m_nFrameType =  (*(m_pBuf+19))&0x1f;
				else
					m_nFrameType =  0x07;
			}
			//保存这次的NAL类型 关键帧
			if ((m_nFrameType == 0x07) || (m_nFrameType == 0x05))
			{
				*nType = 0x01;
			}
			else
			{
				*nType = 0x00;
			}
			//时间戳
			*nTs   = m_RTP_Header.ts;

			//重新初始化保存帧数据的缓冲区
			m_pStart = m_pBuf + 9;
			m_dwSize = 0 ;
			return m_pBuf ;
		}
		else
		{
			return NULL ;
		}
	}
}
//丢包后设置状态标志
void CH264_Rtp_UnPack::SetLostPacket()
{
	m_bLostFlag = 1;
	m_bSPSFound = false;
	m_pStart = m_pBuf + 9;
	m_dwSize = 0 ;
	m_loastPack++;
}
