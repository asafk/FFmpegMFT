#pragma once

#include "IDecoderStrategy.h"

class Decoder 
{
public:
	Decoder();
	~Decoder();

	bool setHWAccelPrefered();

	bool init(std::string codecName);
	bool release();	

	bool decode(unsigned char* in, int in_size, unsigned char*& out, int pitch);
	void flush();

private:
	
	IDecoderStrategy* m_decoderS;
};

