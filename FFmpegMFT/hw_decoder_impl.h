#pragma once
#include "IDecoderStrategy.h"
#include <d3d9.h>

extern "C"
{
	#include <libavcodec/avcodec.h>
}

class hw_decoder_impl : public IDecoderStrategy
{
public:
	hw_decoder_impl();
	~hw_decoder_impl();

	bool init(std::string codecName);
	bool release();	

	bool decode(unsigned char* in, int in_size, void*& surface, int none);
	void flush();

private:
	/*FFmpeg related members*/
	AVCodecContext* m_decoder_ctx;
	AVCodec* m_decoder;
	AVPacket* m_packet;
	AVFrame *m_avFrame;
	enum AVHWDeviceType m_type;


	AVBufferRef *m_hw_device_ctx;
	enum AVPixelFormat m_hw_pix_fmt;

	static AVPixelFormat get_hw_format(AVCodecContext* ctx, const enum AVPixelFormat*	pix_fmts);
	AVPixelFormat get_hw_format_internal(AVCodecContext* ctx, const enum AVPixelFormat*	pix_fmts);

	static int get_buffer(struct AVCodecContext *c, AVFrame *frame, int flags);
	int get_buffer_internal(struct AVCodecContext *c, AVFrame *pic, int flags);


	/*Direct 3d*/
	LPDIRECT3DSURFACE9 m_surface;

};

