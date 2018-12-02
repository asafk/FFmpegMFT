#include "stdafx.h"
#include "Decoder.h"

Decoder::Decoder():	m_avCodec(NULL),
	m_avContext(NULL),
	m_avFrame(NULL),
	m_avPkt(NULL)
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

		ret = avcodec_receive_frame(m_avContext, m_avFrame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			break;
		}
		else if (ret < 0) {
			break;
		}

	} while (false);
}

bool Decoder::decode(unsigned char* in, int in_size, unsigned char*& out, int pitch)
{
	bool bRet = true;
	int ret;

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

	return bRet;
}
