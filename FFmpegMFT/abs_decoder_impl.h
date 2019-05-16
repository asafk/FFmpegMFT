#pragma once
#include "IDecoderStrategy.h"

extern "C"
{
	#include <libavcodec/avcodec.h>
}

class abs_decoder_impl : public IDecoderStrategy
{
public:
	abs_decoder_impl();
	virtual ~abs_decoder_impl();

	bool init(std::string codecName, DWORD pixel_format);
	bool release();	

	bool decode(unsigned char* in, int in_size, void*& out, int pitch) = 0;
	void flush();

	static void log_callback(void* ptr, int level, const char* fmt, va_list vargs);
	void log(int level, const char* fmt, va_list vargs);

protected:
	/*FFmpeg related members*/
	AVCodec *m_avCodec;
	AVCodecContext *m_avContext;
	AVFrame *m_avFrame;
	AVPacket *m_avPkt;

	DWORD m_dwPixelFmt;
};