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

	bool init(std::string codecName);
	bool release();	

	bool decode(unsigned char* in, int in_size, void*& out, int pitch) = 0;
	void flush();

protected:
	/*FFmpeg related members*/
	AVCodec *m_avCodec;
	AVCodecContext *m_avContext;
	AVFrame *m_avFrame;
	AVPacket *m_avPkt;

	bool m_bInit;
};

