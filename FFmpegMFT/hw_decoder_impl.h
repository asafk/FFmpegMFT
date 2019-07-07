#pragma once
#include "abs_decoder_impl.h"
#include <d3d9.h>
#include <dxva2api.h>

extern "C"
{
	#include <libavcodec/avcodec.h>
	#include <libavutil/hwcontext.h>
	#include <libavutil/hwcontext_dxva2.h>
}

class hw_decoder_impl : public abs_decoder_impl
{
public:
	hw_decoder_impl(IDirect3DDeviceManager9* deviceManager9);
	~hw_decoder_impl();

	bool init(std::string codecName, DWORD pixel_format);
	bool release();	

	bool decode(unsigned char* in, int in_size, void*& surface, int none);

private:

	IDirect3DDeviceManager9* m_deviceManager9;

	AVBufferRef *m_hw_device_ctx;
	enum AVPixelFormat m_hw_pix_fmt;

	static AVPixelFormat get_hw_format(AVCodecContext* ctx, const enum AVPixelFormat*	pix_fmts);
	AVPixelFormat get_hw_format_internal(AVCodecContext* ctx, const enum AVPixelFormat*	pix_fmts);
};

