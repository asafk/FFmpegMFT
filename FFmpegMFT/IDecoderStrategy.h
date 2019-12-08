#pragma once
#include <string>

//Last possible errors
#define ERR_DECODE_OK 0
#define	ERR_DECODE_GENERAL -1
#define	ERR_DECODE_NEM -2

class IDecoderStrategy
{
public:
	IDecoderStrategy(){};
	virtual ~IDecoderStrategy(){};

	virtual bool init(std::string codec_name, DWORD pixel_format) = 0;
	virtual bool release() = 0;
	virtual long get_last_error() = 0;

	virtual bool decode(unsigned char*, int, void*&, int) = 0;
	virtual void flush() = 0;
};



