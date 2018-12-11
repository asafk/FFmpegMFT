#pragma once
#include <string>

class IDecoderStrategy
{
public:
	IDecoderStrategy(){};
	virtual ~IDecoderStrategy(){};

	virtual bool init(std::string codecName) = 0;
	virtual bool release() = 0;	

	virtual bool decode(unsigned char* in, int in_size, unsigned char*& out, int pitch) = 0;
	virtual void flush() = 0;
};



