#pragma once
#include "abs_decoder_impl.h"


class cpu_decoder_impl : public abs_decoder_impl
{
public:
	cpu_decoder_impl();
	~cpu_decoder_impl();

	bool decode(unsigned char* in, int in_size, void*& out, int pitch);
};

