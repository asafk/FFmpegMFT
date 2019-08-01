#include "stdafx.h"
#include "abs_decoder_impl.h"


#include <algorithm>

void abs_decoder_impl::log_callback(void* ptr, int level, const char* fmt, va_list vargs)
{
	AVCodecContext* avcl = static_cast<AVCodecContext *>(ptr);
	abs_decoder_impl* this_pointer = static_cast<abs_decoder_impl *>(avcl->opaque);
    if(this_pointer != NULL) this_pointer->log(level, fmt, vargs);
}

void abs_decoder_impl::log(int level, const char* fmt, va_list vargs)
{
	int normalLevel = log4cpp::Priority::NOTSET;
	switch(level)
	{
	case AV_LOG_PANIC:
	case AV_LOG_FATAL:
	case AV_LOG_ERROR:
		normalLevel = log4cpp::Priority::ERROR;
		break;
	case AV_LOG_WARNING:
		normalLevel = log4cpp::Priority::WARN;
		break;
	case AV_LOG_INFO:
	case AV_LOG_VERBOSE:
		normalLevel = log4cpp::Priority::INFO;
		break;		
	case AV_LOG_DEBUG:
	case AV_LOG_TRACE:
		normalLevel = log4cpp::Priority::DEBUG;
		break;
	default:		
		break;
	}

	if(Logger::getInstance().IsPiriorityLevelEnabled(normalLevel, Logger::getInstance().FFmpegLibCategory))
	{
		char dbg_out[512];	
		vsprintf_s(dbg_out, fmt, vargs);

		std::string str("FFmpeg: ");
		str.append(dbg_out);

		str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
		Logger::getInstance().Log(normalLevel, Logger::getInstance().FFmpegLibCategory, str.c_str());	
	}	
}

abs_decoder_impl::abs_decoder_impl():
m_avCodec(NULL),
m_avContext(NULL),
m_avFrame(NULL),
m_avPkt(NULL),
m_dwPixelFmt(0)
{
	av_log_set_callback(log_callback);
}

abs_decoder_impl::~abs_decoder_impl()
{
	abs_decoder_impl::release();
}

bool abs_decoder_impl::init(std::string codecName, DWORD pixel_format)
{
	bool bRet = true;
	
	do
	{
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

		m_avPkt = av_packet_alloc();
		if (m_avPkt == NULL){
			bRet = false;
			break;
		}

		m_avFrame = av_frame_alloc();
		if (m_avFrame == NULL) {
			bRet = false;
			break;
		}

		m_dwPixelFmt = pixel_format;
	}
	while (false);        

	return bRet;
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