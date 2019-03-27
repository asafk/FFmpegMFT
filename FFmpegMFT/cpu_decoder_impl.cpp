#include "stdafx.h"
#include "cpu_decoder_impl.h"

cpu_decoder_impl::cpu_decoder_impl()
{
	Logger::getInstance().LogInfo("Using SW decoding");
}

cpu_decoder_impl::~cpu_decoder_impl()
{
}

bool cpu_decoder_impl::decode(unsigned char* in, int in_size, void*& out, int pitch)
{
	int ret;

	do
	{	
#if USE_BUFFER2
		m_outBuffer = out;
		m_nPitch = pitch;

		m_avContext->opaque = this;
		m_avContext->get_buffer2 = this->get_buffer2;
#endif

		m_avPkt->data = in;
		m_avPkt->size = in_size;

		ret = avcodec_send_packet(m_avContext, m_avPkt);
		if (ret < 0) {
			Logger::getInstance().LogWarn("Error during decoding (avcodec_send_packet)");
			break;
		}			

        ret = avcodec_receive_frame(m_avContext, m_avFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
            break;
		}
        else if (ret < 0) {
			Logger::getInstance().LogWarn("Error during decoding (avcodec_receive_frame)");
			break;
        }

#ifdef USE_BUFFER2
#else
		DWORD height = m_avFrame->height;
		DWORD yStride = m_avFrame->linesize[0];
		DWORD uvStride =  m_avFrame->linesize[1];
		BYTE* pY = m_avFrame->data[0];
		BYTE* pV = m_avFrame->data[1];
		BYTE* pU = m_avFrame->data[2];
		DWORD uvHeight = height / 2;
		LONG uvPitch = pitch / 2;

		for (DWORD row = 0; row < height; row++)
			memcpy((BYTE*)out + row * pitch, &pY[row * yStride], yStride);		

		if(FCC('NV12') == m_dwPixelFmt)
		{
			BYTE* pUVBuffer = (BYTE*)out + height * pitch;

			for (DWORD row = 0; row < uvHeight; row++){
				for(int col = 0; col < uvStride; col++){

					pUVBuffer[row * pitch + col*2] = pV[row * uvStride + col];
					pUVBuffer[row * pitch + col*2 + 1] = pU[row * uvStride + col];
				}
			}
		}
		else if(FCC('YV12') == m_dwPixelFmt)
		{
			BYTE* pUBuffer = (BYTE*)out + height * pitch;
			
			for (DWORD row = 0; row < uvHeight; row++)
				memcpy(pUBuffer + row * uvPitch, &pU[row * uvStride], uvStride);
			
			BYTE* pVBuffer = pUBuffer + height * pitch / 4;
			
			for (DWORD row = 0; row < uvHeight; row++)
				memcpy(pVBuffer + row * uvPitch, &pV[row * uvStride], uvStride);
		}
		else
		{
			Logger::getInstance().LogError("Unknown pixel format &d", m_dwPixelFmt);
		}
#endif
	}
	while (false);	

	return true;
}

#if USE_BUFFER2
int cpu_decoder_impl::get_buffer2(struct AVCodecContext *s, AVFrame *frame, int flags)
{
	cpu_decoder_impl* this_pointer = static_cast<cpu_decoder_impl*>(s->opaque);
    return this_pointer->get_buffer2_internal(s, frame, flags);
}

static void buffer_free(void* opaque, unsigned char* data)
{	
}

int cpu_decoder_impl::get_buffer2_internal(struct AVCodecContext *s, AVFrame *frame, int flags)
{
	int nSize = frame->height - frame->crop_bottom;
	assert(s->codec_type == AVMEDIA_TYPE_VIDEO);
    frame->data[0] = (BYTE*)m_outBuffer;
    frame->data[2] = frame->data[0] + nSize * m_nPitch;
    frame->data[1] = frame->data[2] + nSize / 4 * m_nPitch;
    frame->linesize[0] = m_nPitch;
    frame->linesize[1] = m_nPitch / 2;
    frame->linesize[2] = m_nPitch / 2;
    frame->buf[0] = av_buffer_create(frame->data[0], frame->linesize[0] * nSize, buffer_free, NULL, 0);
    frame->buf[1] = av_buffer_create(frame->data[1], frame->linesize[1] * nSize / 2, buffer_free, NULL, 0);
    frame->buf[2] = av_buffer_create(frame->data[2], frame->linesize[2] * nSize / 2, buffer_free, NULL, 0);
    return 0;
	return avcodec_default_get_buffer2(s,frame,flags);
}
#endif