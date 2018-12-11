#include "stdafx.h"
#include "hw_decoder_impl.h"


hw_decoder_impl::hw_decoder_impl()
{
}


hw_decoder_impl::~hw_decoder_impl()
{
}

bool hw_decoder_impl::init(std::string codecName)
{
	throw new std::exception("NotImplement");
}

bool hw_decoder_impl::release()
{
	throw new std::exception("NotImplement");
}

void hw_decoder_impl::flush()
{
	throw new std::exception("NotImplement");
}	

bool hw_decoder_impl::decode(unsigned char* in, int in_size, unsigned char*& out, int pitch)
{
	throw new std::exception("NotImplement");
}