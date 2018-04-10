/*
 * H264RTP_UnPack.h
 *
 *  Created on: 2017年8月7日
 *      Author: root
 */

#ifndef H264RTP_UNPACK_H_
#define H264RTP_UNPACK_H_


class CH264_Rtp_UnPack
{
#define RTP_VERSION 2
#define BUF_SIZE	(1024 * 1024)
	//RTP信息头定义
	typedef struct{
		//LITTLE_ENDIAN 
		unsigned short	cc:4;	/* CSRC count				*/
		unsigned short	 x:1;	/* header extension flag	*/
		unsigned short	 p:1;	/* padding flag				*/
		unsigned short	 v:2;	/* packet type				*/
		unsigned short	pt:7;	/* payload type				*/
		unsigned short	 m:1;	/* marker bit				*/
		unsigned short	 seq;	/* sequence number			*/
		unsigned long	  ts;	/* timestamp				*/
		unsigned long	ssrc;	/*synchronization source	*/
	}Rtp_Hdr_t;
public:
	//构造函数
	CH264_Rtp_UnPack(void* hr, unsigned char H264PayLoadType = 96);
	//析构函数
	~CH264_Rtp_UnPack(void);
	//设置数据缓冲区
	void SetBuffer(unsigned char *pBuf, int nSize);
	//pBuf为H264 RTP视频数据包，nSize为RTP视频数据包字节长度，outSize为输出视频数据帧字节长度。
	//返回值为指向视频数据帧的指针。输入数据可能被破坏。
	unsigned char* Parse_RTP_Packet ( unsigned char *pBuf, unsigned short nSize, int *outSize ,int *nType,unsigned int *nSeq, unsigned int *nSSRC,unsigned long *nTs, unsigned char* nLostFlag);
	//丢包后设置状态标志
	void SetLostPacket ();
	unsigned int m_loastPack;
	unsigned char 	m_H264PayLoadType ;
private:
	Rtp_Hdr_t	m_RTP_Header;
	unsigned int m_ssrc ;
	unsigned char*	m_pBuf ;

	unsigned char*	m_pStart ;
	unsigned char*	m_pEnd;
	unsigned int	m_dwSize ;

	unsigned char 	m_nNALUnitType;
	bool	m_bSPSFound;
	unsigned char 	m_nFrameType;
	unsigned char 	m_bBeginsFrame;
	unsigned char 	m_bCompletesFrame;
	unsigned char    m_bLostFlag;

	short	m_wSeq ;
	unsigned char	* pPayload;
	unsigned int	PayloadSize;
	unsigned int	seqNumCycle;

};
// class CH264_RTP_UNPACK end 
/*
//使用范例
HRESULT hr ; 
CH264_RTP_UNPACK unpack ( hr ) ; 
BYTE *pRtpData ; 
WORD inSize; 
int outSize ; 
int nType;
BYTE *pFrame = unpack.Parse_RTP_Packet ( pRtpData, inSize, &outSize , &nType) ; 
if ( pFrame != NULL ) 
{ 
// frame process 
// ... 
} 
*/



#endif /* H264RTP_UNPACK_H_ */
