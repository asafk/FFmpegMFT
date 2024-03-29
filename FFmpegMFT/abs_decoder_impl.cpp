#include "stdafx.h"
#include "abs_decoder_impl.h"

abs_decoder_impl::abs_decoder_impl():
m_avCodec(NULL),
m_avContext(NULL),
m_avFrame(NULL),
m_avPkt(NULL),
m_dwPixelFmt(0),
m_lLastErr(ERR_DECODE_OK)
{
}

abs_decoder_impl::~abs_decoder_impl()
{
	abs_decoder_impl::release();
}

bool abs_decoder_impl::release()
{
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

	m_avCodec = NULL;

	return true;
}

long abs_decoder_impl::get_last_error()
{
	return m_lLastErr;
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