#pragma once
#include <string>

extern "C"
{
	#include <libavcodec/avcodec.h>
	#include <libavutil/imgutils.h>
	#include <libavutil/timestamp.h>
	#include <libavformat/avformat.h>
}

using namespace std;

class Decoder
{
public:
	Decoder();
	~Decoder();

	bool init(string codecName);
	bool release();	

	bool decode(unsigned char* in, int in_size, unsigned char*& out, int pitch);
	void flush();

private:
	/*FFmpeg related members*/
	AVCodec *m_avCodec;
	AVCodecContext *m_avContext;
	AVFrame *m_avFrame;
	AVPacket *m_avPkt;
};

