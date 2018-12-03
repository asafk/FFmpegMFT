#include "stdafx.h"
#include "Decoder.h"
#include <cassert>

Decoder::Decoder():	m_avCodec(NULL),
	m_avContext(NULL),
	m_avFrame(NULL),
	m_avPkt(NULL)/*,
	m_pData(NULL)*/
{
	avcodec_register_all();
}

Decoder::~Decoder()
{
}

bool Decoder::init(string codecName)
{
	bool bRet = true;	
	
	do
	{
		m_avPkt = av_packet_alloc();
		if (m_avPkt == NULL){
			bRet = false;
			break;
		}

		/* find the video decoder according to codecName*/
		/* currently support only HEVC or H.264*/
		if(codecName.compare("HEVC") == 0)
			m_avCodec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
		else
			m_avCodec = avcodec_find_decoder(AV_CODEC_ID_H264);

	    if (m_avCodec == NULL) {
			bRet = false;
			break;
	    }		
		
		m_avContext = avcodec_alloc_context3(m_avCodec);
		if (m_avContext == NULL){
			bRet = false;
			break;
		}

		/* open it */
	    if (avcodec_open2(m_avContext, m_avCodec, NULL) < 0) {
	        bRet = false;
			break;
	    }

		m_avFrame = av_frame_alloc();
		if (m_avFrame == NULL) {
			bRet = false;
			break;
		}

	}
	while (false);
        

	return bRet;
}

bool Decoder::release()
{
	flush();

	if(m_avContext != NULL) {
		avcodec_free_context(&m_avContext);
		m_avContext = NULL;
	}

	if(m_avFrame != NULL) {
		av_frame_free(&m_avFrame);
		m_avFrame = NULL;
	}

	if(m_avPkt != NULL)	{
		av_packet_free(&m_avPkt);
		m_avPkt = NULL;
	}

	return true;
}

void Decoder::flush()
{
	do
	{
		int ret = avcodec_send_packet(m_avContext, NULL);
		if (ret < 0) {
		    break;
		}
		while (ret >= 0) 
		{
		    ret = avcodec_receive_frame(m_avContext, m_avFrame);
		    if (ret == AVERROR_EOF)
		        break;
		}
	} while (false);

	avcodec_flush_buffers(m_avContext);
}

//int Decoder::my_get_buffer(struct AVCodecContext* c, AVFrame* frame, int flags)
//{
//	auto this_pointer = static_cast<Decoder*>(c->opaque);
//    return this_pointer->my_get_buffer_real(c, frame, flags);
//}
//
//void our_buffer_default_free(void *opaque, uint8_t *data)
//{
//    // empty
//	av_buffer_default_free(opaque,data);
//}
//int Decoder::my_get_buffer_real(struct AVCodecContext* c, AVFrame* pic, int flags)
//{
//	assert(c->codec_type == AVMEDIA_TYPE_VIDEO);
//
//	//m_pitch= pic->width;
//
//	avcodec_default_get_buffer2(c,pic,flags);
//    //pic->data[0] = m_pData;
//    //pic->data[2] = m_pData + m_pitch * pic->height / 2;
//    //pic->data[1] = m_pData ;//+ m_pitch * pic->height / 2 + m_pitch * pic->height / 4;
//    //pic->linesize[0] = m_pitch;
//   // pic->linesize[2] = m_pitch;// / 2;
//    //pic->linesize[1] = m_pitch;// / 2;
//    //pic->buf[0] = av_buffer_create(pic->data[0], pic->linesize[0] * pic->height , our_buffer_default_free, NULL, AV_BUFFER_FLAG_READONLY);
//    //pic->buf[2] = av_buffer_create(pic->data[1], pic->linesize[1] * pic->height / 2, our_buffer_default_free, NULL, 0);
//   // pic->buf[1] = av_buffer_create(pic->data[2], pic->linesize[2] * pic->height / 2, our_buffer_default_free, NULL, 0);
//
//	pic->extended_data = pic->data;
//
//    return 0;
//}
enum AVPixelFormat Decoder::get_format(struct AVCodecContext *s, const enum AVPixelFormat * fmt)
{
	return AV_PIX_FMT_YUV420P;
}

bool Decoder::decode(unsigned char* in, int in_size, unsigned char*& out, int pitch)
{
	bool bRet = true;
	int ret;

	/*m_pitch = pitch;
	m_pData = out;
	m_avContext->opaque = this;
	m_avContext->get_buffer2 = my_get_buffer;*/
	m_avContext->get_format = get_format;
	

	do
	{	
		m_avPkt->data = in;
		m_avPkt->size = in_size;

		ret = avcodec_send_packet(m_avContext, m_avPkt);
		if (ret < 0) {
			bRet = false;
			break;
		}

        ret = avcodec_receive_frame(m_avContext, m_avFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
			bRet = false;
            break;
		}
        else if (ret < 0) {
            bRet = false;
			break;
        }

	}
	while (false);

	if(bRet == true)
	{
		if(m_avFrame->format == AV_PIX_FMT_YUV420P){
		DWORD height = m_avFrame->height;
		DWORD yStride = m_avFrame->width;
		DWORD uvStride =  m_avFrame->width / 2;
		BYTE* pY = m_avFrame->data[0];
		BYTE* pV = m_avFrame->data[2];
		BYTE* pU = m_avFrame->data[1];
		DWORD uvHeight = height / 2;
		LONG uvPitch = pitch / 2;

		for (DWORD row = 0; row < height; row++)
			memcpy(out + row * pitch, &pY[row * yStride], yStride);

		BYTE* pVBuffer = out + height * pitch;

		for (DWORD row = 0; row < uvHeight; row++)
			memcpy(pVBuffer + row * uvPitch, &pV[row * uvStride], uvStride);

		BYTE* pUBuffer = pVBuffer + height * pitch / 4;

		for (DWORD row = 0; row < uvHeight; row++)
			memcpy(pUBuffer + row * uvPitch, &pU[row * uvStride], uvStride);
		}
	}

	return bRet;
}
