#include "stdafx.h"
#include "cpu_decoder_impl.h"

cpu_decoder_impl::cpu_decoder_impl()
{
}

cpu_decoder_impl::~cpu_decoder_impl()
{
}

bool cpu_decoder_impl::decode(unsigned char* in, int in_size, void*& out, int pitch)
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
		if(m_avFrame->format == AV_PIX_FMT_YUV420P ||
			m_avFrame->format == AV_PIX_FMT_YUVJ420P){
			DWORD height = m_avFrame->height;
			DWORD yStride = m_avFrame->width;
			DWORD uvStride =  m_avFrame->width / 2;
			BYTE* pY = m_avFrame->data[0];
			BYTE* pV = m_avFrame->data[2];
			BYTE* pU = m_avFrame->data[1];
			DWORD uvHeight = height / 2;
			LONG uvPitch = pitch / 2;

			for (DWORD row = 0; row < height; row++)
				memcpy((BYTE*)out + row * pitch, &pY[row * yStride], yStride);

			BYTE* pVBuffer = (BYTE*)out + height * pitch;

			for (DWORD row = 0; row < uvHeight; row++)
				memcpy(pVBuffer + row * uvPitch, &pV[row * uvStride], uvStride);

			BYTE* pUBuffer = pVBuffer + height * pitch / 4;

			for (DWORD row = 0; row < uvHeight; row++)
				memcpy(pUBuffer + row * uvPitch, &pU[row * uvStride], uvStride);
		}
	}

	return bRet;
}