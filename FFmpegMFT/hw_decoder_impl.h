#pragma once
#include "IDecoderStrategy.h"

class hw_decoder_impl : public IDecoderStrategy
{
public:
	hw_decoder_impl();
	~hw_decoder_impl();

	bool init(std::string codecName);
	bool release();	

	bool decode(unsigned char* in, int in_size, unsigned char*& out, int pitch);
	void flush();

};

