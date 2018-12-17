#include "stdafx.h"
#include "Decoder.h"
#include "cpu_decoder_impl.h"
#include "hw_decoder_impl.h"

Decoder::Decoder()
{
	m_decoderS = new cpu_decoder_impl();
}

Decoder::~Decoder()
{
	SAFE_DELETE(m_decoderS);
}

bool Decoder::setDecoderStrategy(IDecoderStrategy* strategy)
{
	if(strategy != NULL){
		SAFE_DELETE(m_decoderS);
		m_decoderS = strategy;
	}

	return m_decoderS != NULL ? true : false;
}

bool Decoder::init(std::string codecName)
{
	return m_decoderS != NULL ?  m_decoderS->init(codecName) : false;
}

bool Decoder::release()
{
	return m_decoderS != NULL ?  m_decoderS->release() : false;
}

void Decoder::flush()
{
	m_decoderS->flush();
}

bool Decoder::decode(unsigned char* in, int in_size, void*& out, int pitch)
{
	return m_decoderS != NULL ? m_decoderS->decode(in,in_size,out,pitch) : false;
}
