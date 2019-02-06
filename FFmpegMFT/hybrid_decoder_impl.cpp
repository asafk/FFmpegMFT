#include "stdafx.h"
#include "hybrid_decoder_impl.h"
#include <chrono>


hybrid_decoder_impl::hybrid_decoder_impl(): 
m_type(AV_HWDEVICE_TYPE_NONE),
m_hw_device_ctx(NULL),
m_hw_pix_fmt(AV_PIX_FMT_NONE),
m_avHWFrame(NULL)
{
	Logger::getInstance().LogInfo("Created Hybrid decoder");
}

hybrid_decoder_impl::~hybrid_decoder_impl()
{
}

bool hybrid_decoder_impl::init(std::string codecName, DWORD pixel_format)
{
	bool bRet = true;

	do
	{
		m_type = av_hwdevice_find_type_by_name("dxva2");
	    if (m_type == AV_HWDEVICE_TYPE_NONE) {
	        Logger::getInstance().LogWarn("Device type %s is not supported.", "dxva2");
	        Logger::getInstance().LogInfo("Available device types:");
	        while((m_type = av_hwdevice_iterate_types(m_type)) != AV_HWDEVICE_TYPE_NONE)
	            Logger::getInstance().LogInfo(" %s", av_hwdevice_get_type_name(m_type));

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

		for (int i = 0;; i++) {
	        const AVCodecHWConfig *config = avcodec_get_hw_config(m_avCodec, i);
	        if (!config) {
	            Logger::getInstance().LogWarn("Decoder %s does not support device type %s.",
	                    m_avCodec->name, av_hwdevice_get_type_name(m_type));
	            bRet = false;
	        	break;
	        }
	        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
	            config->device_type == m_type) {
	            m_hw_pix_fmt = config->pix_fmt;
	            break;
	        }
		}

		if(bRet == false)
			break;

		if (!(m_avContext = avcodec_alloc_context3(m_avCodec))){
			Logger::getInstance().LogWarn("Failed to create context for Decoder %s.",m_avCodec->name);
			bRet = false;
			break;
		}

		m_avContext->opaque = this;
		m_avContext->get_format = get_hw_format;

	    if ((av_hwdevice_ctx_create(&m_hw_device_ctx, m_type,
	                                      NULL, NULL, 0)) < 0) {
	    	Logger::getInstance().LogWarn("Failed to create specified HW device.");
	    	bRet = false;
	    	break;
	    }
	    m_avContext->hw_device_ctx = av_buffer_ref(m_hw_device_ctx);

		if ((avcodec_open2(m_avContext, m_avCodec, NULL)) < 0) {
	        Logger::getInstance().LogWarn("Failed to open codec for stream.");
			bRet = false;
	    	break;
		}

		m_avPkt = av_packet_alloc();
		if (m_avPkt == NULL){
			bRet = false;
			break;
		}

		m_avFrame = av_frame_alloc();
		if (m_avFrame == NULL) {
			bRet = false;
			break;
		}

		m_avHWFrame = av_frame_alloc();
		if (m_avHWFrame == NULL) {
			bRet = false;
			break;
		}
		m_dwPixelFmt = pixel_format;
	}
	while (false);

	return bRet;
}

bool hybrid_decoder_impl::release()
{
	if(m_hw_device_ctx != NULL){
		av_buffer_unref(&m_hw_device_ctx);
		m_hw_device_ctx = NULL;
	}

	if(m_avHWFrame != NULL) {
		av_frame_free(&m_avHWFrame);
		m_avHWFrame = NULL;
	}

	return abs_decoder_impl::release();
}

bool hybrid_decoder_impl::decode(unsigned char* in, int in_size, void*& out, int pitch)
{
	bool bRet = true;
	bool bGpuDecode = true;
	int ret;

	do
	{	
		m_avPkt->data = in;
		m_avPkt->size = in_size;

		ret = avcodec_send_packet(m_avContext, m_avPkt);
		if (ret < 0) {
			Logger::getInstance().LogWarn("Error during decoding (avcodec_send_packet)");
			bRet = false;
			break;
		}		

        ret = avcodec_receive_frame(m_avContext, m_avHWFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
			bRet = false;
            break;
		}
        else if (ret < 0) {
			Logger::getInstance().LogWarn("Error during decoding (avcodec_receive_frame)");
            bRet = false;
			break;
        }

		if (m_avHWFrame->format == m_hw_pix_fmt) {
			/* retrieve data from GPU to CPU */
			if ((ret = av_hwframe_transfer_data(m_avFrame, m_avHWFrame, 0)) < 0) {
				Logger::getInstance().LogWarn("Error transferring the data to system memory");
				bRet = false;
				break;
			}
		} else {
			m_avFrame = m_avHWFrame;
			bGpuDecode = false;
			Logger::getInstance().LogDebug("hybrid_decoder_impl::decode fallback to CPU only format=%d",m_avHWFrame->format);
		}

	}
	while (false);

	if(bRet == true)
	{
		DWORD height = m_avFrame->height;
		DWORD yStride = m_avFrame->width;
		DWORD uvStride =  m_avFrame->width / 2;
		BYTE* pY = m_avFrame->data[0];

		//Interleave UV from GPU
		BYTE* pUV = m_avFrame->data[1];
		//or 2 planer from CPU
		BYTE* pV = m_avFrame->data[1];
		BYTE* pU = m_avFrame->data[2];

		DWORD uvHeight = height / 2;
		LONG uvPitch = pitch / 2;

		for (DWORD row = 0; row < height; row++)
			memcpy((BYTE*)out + row * pitch, &pY[row * yStride], yStride);		

		if(FCC('NV12') == m_dwPixelFmt) {
			BYTE* pUVBuffer = (BYTE*)out + height * pitch;

			for (DWORD row = 0; row < uvHeight; row++){
				for(int col = 0; col < m_avFrame->linesize[1]; col++){
					if(bGpuDecode){
						pUVBuffer[row * pitch + col] = pUV[row * yStride + col];
					} else {
						pUVBuffer[row * pitch + col*2] = pV[row * uvStride + col];
						pUVBuffer[row * pitch + col*2 + 1] = pU[row * uvStride + col];
					}
				}
			}
		}
		else if(FCC('YV12') == m_dwPixelFmt) {
			BYTE* pUBuffer = (BYTE*)out + height * pitch;

			if(bGpuDecode){
				for (DWORD row = 0; row < uvHeight; row++){
					for(int col = 0; col < m_avFrame->linesize[1]; col++){
						if(col % 2 != 0) pUBuffer[row * uvPitch + col/2] = pUV[row * yStride + col];
					}
				}
			} else {
				for (DWORD row = 0; row < uvHeight; row++)
					memcpy(pUBuffer + row * uvPitch, &pU[row * uvStride], uvStride);
			}

			BYTE* pVBuffer = pUBuffer + height * pitch / 4;

			if(bGpuDecode){
				for (DWORD row = 0; row < uvHeight; row++){
					for(int col = 0; col < m_avFrame->linesize[1]; col++){
						if(col % 2 == 0) pVBuffer[row * uvPitch + col/2] = pUV[row * yStride + col];
					}
				}
			} else {
				for (DWORD row = 0; row < uvHeight; row++)
					memcpy(pVBuffer + row * uvPitch, &pV[row * uvStride], uvStride);
			}
		} else {
			Logger::getInstance().LogError("Unknown pixel format &d", m_dwPixelFmt);
		}
	}

	return bRet;
}

AVPixelFormat hybrid_decoder_impl::get_hw_format(AVCodecContext* ctx, const AVPixelFormat* pix_fmts)
{
	hybrid_decoder_impl* this_pointer = static_cast<hybrid_decoder_impl*>(ctx->opaque);
    return this_pointer->get_hw_format_internal(ctx, pix_fmts);
}

AVPixelFormat hybrid_decoder_impl::get_hw_format_internal(AVCodecContext* ctx, const AVPixelFormat* pix_fmts)
{
	const enum AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++) {
        if (*p == m_hw_pix_fmt)
            return *p;
    }
    Logger::getInstance().LogError("Failed to get HW surface format.");
    
	/* Fallback to default behaviour */
    return avcodec_default_get_format( ctx, pix_fmts );
}
