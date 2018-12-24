#include "stdafx.h"
#include "cpu_decoder_impl.h"
#include <cassert>

#include "utils.h"

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
#if USE_BUFFER2
		m_outBuffer = out;
		m_nPitch = pitch;

		m_avContext->opaque = this;
		m_avContext->get_buffer2 = this->get_buffer2;
#endif

		m_avPkt->data = in;
		m_avPkt->size = in_size;

		auto t1 = std::chrono::steady_clock::now();
		ret = avcodec_send_packet(m_avContext, m_avPkt);
		if (ret < 0) {
			bRet = false;
			break;
		}
		auto t2 = std::chrono::steady_clock::now();		

        ret = avcodec_receive_frame(m_avContext, m_avFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
			bRet = false;
            break;
		}
        else if (ret < 0) {
            bRet = false;
			break;
        }

		auto t3 = std::chrono::steady_clock::now();	

		DebugOut((L"Dec send=%f ms rec=%f ms\n"),
			std::chrono::duration <double, std::milli> (t2-t1).count(),
			std::chrono::duration <double, std::milli> (t3-t2).count());

	}
	while (false);

	if(bRet == true)
	{
#ifndef USE_BUFFER2
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
#endif
	}

	return bRet;
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