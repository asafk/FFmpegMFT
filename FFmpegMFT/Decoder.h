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

	//int m_pitch;
	//unsigned char* m_pData;

	//static int my_get_buffer(struct AVCodecContext *c, AVFrame *frame, int flags);
	//int my_get_buffer_real(struct AVCodecContext *c, AVFrame *pic, int flags);
	static enum AVPixelFormat get_format(struct AVCodecContext *s, const enum AVPixelFormat * fmt);
};

