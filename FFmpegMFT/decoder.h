#pragma once

#include "IDecoderStrategy.h"

class Decoder 
{
public:
	Decoder();
	~Decoder();

	bool setDecoderStrategy(IDecoderStrategy* strategy);

	bool init(std::string codecName);
	bool release();	
	/*
	 * decode can used by HW or SW 
	 * SW :
	 *	decode(unsigned char* in, int in_size, BYTE*& out_buf, int pitch)
	 * HW :
	 *	decode(unsigned char* in, int in_size, void*& surface, int none)
	 */
	bool decode(unsigned char*, int, void*&, int);
	void flush();

private:
	
	IDecoderStrategy* m_decoderS;
};

