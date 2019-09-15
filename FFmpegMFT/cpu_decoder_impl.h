#pragma once
#include "abs_decoder_impl.h"


class cpu_decoder_impl : public abs_decoder_impl
{
public:
	cpu_decoder_impl();
	~cpu_decoder_impl();

	bool init(std::string codecName, DWORD pixel_format);
	bool decode(unsigned char* in, int in_size, void*& out, int pitch);
#if USE_BUFFER2
protected:

	static int get_buffer2(struct AVCodecContext *c, AVFrame *pic, int flags);
	int get_buffer2_internal(struct AVCodecContext *c, AVFrame *pic, int flags);

	void* m_outBuffer;
	int m_nPitch;
#endif
};

