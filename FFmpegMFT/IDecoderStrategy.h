#pragma once
#include <string>

class IDecoderStrategy
{
public:
	IDecoderStrategy(){};
	virtual ~IDecoderStrategy(){};

	virtual bool init(std::string codecName, DWORD pixel_format) = 0;
	virtual bool release() = 0;	

	virtual bool decode(unsigned char*, int, void*&, int) = 0;
	virtual void flush() = 0;
};



