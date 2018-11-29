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

	bool decode(unsigned char* in, int in_size, unsigned char*& out, int* out_size);

private:
	/*FFmpeg related members*/
	AVCodec *m_avCodec;
	AVCodecContext *m_avContext;
	AVFrame *m_avFrame;
	AVPacket *m_avPkt;

	//helper function
	//void pgm_save(unsigned char *buf, int wrap, int xsize, int ysize, char *filename)
	//{
	//	FILE *f;
	//    int i;
	//    f = fopen(filename,"wb");
	//	
	//    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
	//    for (i = 0; i < ysize; i++)
	//        fwrite(buf + i * wrap, 1, xsize, f);
	//    fclose(f);
	//}
};

