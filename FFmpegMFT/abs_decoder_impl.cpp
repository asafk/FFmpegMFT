#include "stdafx.h"
#include "abs_decoder_impl.h"


abs_decoder_impl::abs_decoder_impl():
m_avCodec(NULL),
m_avContext(NULL),
m_avFrame(NULL),
m_avPkt(NULL),
m_bInit(false)
{
}

abs_decoder_impl::~abs_decoder_impl()
{
	release();
}

bool abs_decoder_impl::init(std::string codecName)
{
	bool bRet = true;

	//validation
	if(m_bInit)
		return bRet;
	
	do
	{
		m_avPkt = av_packet_alloc();
		if (m_avPkt == NULL){
			bRet = false;
			break;
		}

		/* find the video decoder according to codecName*/
		/* currently support only HEVC or H.264*/
		if(codecName.compare("HEVC") == 0)
			m_avCodec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
		else
			m_avCodec = avcodec_find_decoder(AV_CODEC_ID_H264);

	    if (m_avCodec == NULL) {
			bRet = false;
			break;
	    }		
		
		m_avContext = avcodec_alloc_context3(m_avCodec);
		if (m_avContext == NULL){
			bRet = false;
			break;
		}

		/* open it */
	    if (avcodec_open2(m_avContext, m_avCodec, NULL) < 0) {
	        bRet = false;
			break;
	    }

		m_avFrame = av_frame_alloc();
		if (m_avFrame == NULL) {
			bRet = false;
			break;
		}

	}
	while (false);        

	return m_bInit = bRet;
}

bool abs_decoder_impl::release()
{
	if(!m_bInit)
		return true;

	flush();

	if(m_avContext != NULL) {
		avcodec_free_context(&m_avContext);
		m_avContext = NULL;
	}

	if(m_avFrame != NULL) {
		av_frame_free(&m_avFrame);
		m_avFrame = NULL;
	}

	if(m_avPkt != NULL)	{
		av_packet_free(&m_avPkt);
		m_avPkt = NULL;
	}

	return m_bInit = false;
}

void abs_decoder_impl::flush()
{
	do
	{
		if(m_avContext == NULL || m_avFrame == NULL)
			return;

		int ret = avcodec_send_packet(m_avContext, NULL);
		if (ret < 0) {
		    break;
		}
		while (ret >= 0) 
		{
		    ret = avcodec_receive_frame(m_avContext, m_avFrame);
		    if (ret == AVERROR_EOF)
		        break;
		}
	} while (false);

	avcodec_flush_buffers(m_avContext);
}