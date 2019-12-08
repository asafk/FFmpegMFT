#include "stdafx.h"
#include "cpu_decoder_impl.h"

cpu_decoder_impl::cpu_decoder_impl()
{
	Logger::getInstance().LogInfo("Mode SW is active");
}

cpu_decoder_impl::~cpu_decoder_impl()
{
}

bool cpu_decoder_impl::init(std::string codecName, DWORD pixel_format)
{
	bool bRet = true;
	
	do
	{
		/* find the video decoder according to codecName*/
		/* currently support only HEVC or H.264*/
		if(codecName.compare("HEVC") == 0)
			m_avCodec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
		else
			m_avCodec = avcodec_find_decoder(AV_CODEC_ID_H264);

	    if (m_avCodec == NULL) {
	    	Logger::getInstance().LogError("Failed to to find decoder for codec %s.", codecName);
			bRet = false;
			break;
	    }		
		
		m_avContext = avcodec_alloc_context3(m_avCodec);
		if (m_avContext == NULL){
			Logger::getInstance().LogError("Failed to create context for Decoder %s.", m_avCodec->name);
			bRet = false;
			break;
		}

		m_avContext->opaque = this;
		m_avContext->get_buffer2 = get_buffer2;

		/* open it */
	    if (avcodec_open2(m_avContext, m_avCodec, NULL) < 0) {
	    	Logger::getInstance().LogError("Failed to open codec for stream.");
	        bRet = false;
			break;
	    }

		m_avPkt = av_packet_alloc();
		if (m_avPkt == NULL){
			Logger::getInstance().LogError("Cannot allocate av_packet.");
			bRet = false;
			break;
		}

		m_avFrame = av_frame_alloc();
		if (m_avFrame == NULL) {
			Logger::getInstance().LogError("Cannot allocate av_frame.");
			bRet = false;
			break;
		}

		m_dwPixelFmt = pixel_format;

		Logger::getInstance().LogInfo("SW CodecContext @ %p", (void*)m_avContext);
	}
	while (false);        

	return bRet;
}

bool cpu_decoder_impl::decode(unsigned char* in, int in_size, void*& out, int pitch)
{
	//Initialization check
	if(m_avPkt == NULL || m_avContext == NULL || m_avFrame == NULL)
	{
		Logger::getInstance().LogError("SW mode decoder not initialized");
		return false;
	}

	bool bRet = true;
	int ret = 0;

	do
	{	
		m_avPkt->data = in;
		m_avPkt->size = in_size;

		ret = avcodec_send_packet(m_avContext, m_avPkt);
		if (ret < 0) {
			Logger::getInstance().LogWarn("Error during SW decoding (avcodec_send_packet)");
			bRet = false;
			break;
		}			

        ret = avcodec_receive_frame(m_avContext, m_avFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
            break;
		}
        else if (ret < 0) {
			Logger::getInstance().LogWarn("Error during SW decoding (avcodec_receive_frame)");
			break;
        }

		DWORD height = m_avFrame->height;
		DWORD yStride = m_avFrame->linesize[0];
		DWORD uvStride =  m_avFrame->linesize[1];
		BYTE* pY = m_avFrame->data[0];
		BYTE* pV = m_avFrame->data[1];
		BYTE* pU = m_avFrame->data[2];
		DWORD uvHeight = height / 2;
		LONG uvPitch = pitch / 2;

		if(FCC('NV12') == m_dwPixelFmt || FCC('YV12') == m_dwPixelFmt)
		{
			Logger::getInstance().LogDebug("cpu_decoder_impl::decode (NV12) || (YV12)");

			for (DWORD row = 0; row < height; row++)
				memcpy((BYTE*)out + row * pitch, &pY[row * yStride], yStride);

			if(FCC('NV12') == m_dwPixelFmt)
			{
				Logger::getInstance().LogDebug("cpu_decoder_impl::decode (NV12)");

				BYTE* pUVBuffer = (BYTE*)out + height * pitch;

				for (DWORD row = 0; row < uvHeight; row++){
					for(int col = 0; col < uvStride; col++){

						pUVBuffer[row * pitch + col*2] = pV[row * uvStride + col];
						pUVBuffer[row * pitch + col*2 + 1] = pU[row * uvStride + col];
					}
				}
			}
			else/* if(FCC('YV12') == m_dwPixelFmt)*/
			{
				Logger::getInstance().LogDebug("cpu_decoder_impl::decode (YV12)");

				BYTE* pUBuffer = (BYTE*)out + height * pitch;
				
				for (DWORD row = 0; row < uvHeight; row++)
					memcpy(pUBuffer + row * uvPitch, &pU[row * uvStride], uvStride);
				
				BYTE* pVBuffer = pUBuffer + height * pitch / 4;
				
				for (DWORD row = 0; row < uvHeight; row++)
					memcpy(pVBuffer + row * uvPitch, &pV[row * uvStride], uvStride);
			}
		}
		else if(FCC('YUY2') == m_dwPixelFmt)
		{
			Logger::getInstance().LogDebug("cpu_decoder_impl::decode (YUY2)");
			BYTE* pYUYVBuffer = (BYTE*)out;
			for (DWORD row = 0; row < height; row++){
				for(DWORD col = 0; col < yStride; col++){
					pYUYVBuffer[row * pitch + col*2] = pY[row * yStride + col];
				}			
				for(int col = 0; col < uvStride; col++){

					pYUYVBuffer[row * pitch + col*4 + 1] = pV[row / 2 * uvStride + col];
					pYUYVBuffer[row * pitch + col*4 + 3] = pU[row / 2 * uvStride + col];
				}
			}
		}
		else
		{
			Logger::getInstance().LogError("Unknown pixel format &d", m_dwPixelFmt);
		}
		
		m_lLastErr = ERR_DECODE_OK;
	}
	while (false);	

	return bRet;
}

int cpu_decoder_impl::get_buffer2(struct AVCodecContext *s, AVFrame *frame, int flags)
{
	cpu_decoder_impl* this_pointer = static_cast<cpu_decoder_impl*>(s->opaque);
    return this_pointer->get_buffer2_internal(s, frame, flags);
}

int cpu_decoder_impl::get_buffer2_internal(struct AVCodecContext *s, AVFrame *frame, int flags)
{
	const int nRet = avcodec_default_get_buffer2(s,frame,flags);
	if(nRet < 0)
	{
		Logger::getInstance().LogWarn("Out of memory reached, can't allocate next buffer");
		m_lLastErr = ERR_DECODE_NEM;
	}

	return nRet;
}