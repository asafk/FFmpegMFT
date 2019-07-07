#pragma once
#include "abs_decoder_impl.h"
#include <d3d9.h>

extern "C"
{
	#include <libavcodec/avcodec.h>
	#include <libavutil/hwcontext.h>
}


//this decoder is GPU to CPU, decode on the GPU and pass surface to CPU memory
class hybrid_decoder_impl : public abs_decoder_impl
{
public:
	hybrid_decoder_impl();
	~hybrid_decoder_impl();

	bool init(std::string codecName, DWORD pixel_format);
	bool release();	

	bool decode(unsigned char* in, int in_size, void*& surface, int none);

private:

	AVFrame *m_avHWFrame;

	AVBufferRef *m_hw_device_ctx;
	enum AVPixelFormat m_hw_pix_fmt;

	static AVPixelFormat get_hw_format(AVCodecContext* ctx, const enum AVPixelFormat*	pix_fmts);
	AVPixelFormat get_hw_format_internal(AVCodecContext* ctx, const enum AVPixelFormat*	pix_fmts);
};

