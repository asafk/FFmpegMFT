#include "stdafx.h"
#include "hw_decoder_impl.h"
#include "dbg.h"
#include "Utils.h"


hw_decoder_impl::hw_decoder_impl(IDirect3DDeviceManager9* deviceManager9):
m_deviceManager9(deviceManager9),
m_hw_device_ctx(NULL),
m_hw_pix_fmt(AV_PIX_FMT_NONE),
m_numOfSurfaces(10)
{
	Logger::getInstance().LogInfo("Mode HW is active");
}

hw_decoder_impl::~hw_decoder_impl()
{
	hw_decoder_impl::release();
}

bool hw_decoder_impl::init(std::string codecName, DWORD pixel_format)
{
	bool bRet = true;

	do
	{
		AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
		type = av_hwdevice_find_type_by_name("dxva2");
	    if (type == AV_HWDEVICE_TYPE_NONE) {
	        Logger::getInstance().LogWarn("Device type %s is not supported.", "dxva2");
	        Logger::getInstance().LogInfo("Available device types:");
	        while((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
	            Logger::getInstance().LogInfo(" %s", av_hwdevice_get_type_name(type));

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
	            Logger::getInstance().LogError("Decoder %s does not support device type %s.",
	                    m_avCodec->name, av_hwdevice_get_type_name(type));
	            return false;
	        }
	        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
	            config->device_type == type) {
	            m_hw_pix_fmt = config->pix_fmt;
	            break;
	        }
		}

		if (!(m_avContext = avcodec_alloc_context3(m_avCodec))){
			Logger::getInstance().LogError("Failed to create context for Decoder %s.", m_avCodec->name);
			bRet = false;
			break;
		}

		m_avContext->opaque = this;
		m_avContext->get_format = get_hw_format;

		/* associate the device manager device to FFmpeg device */
		if(m_deviceManager9 == NULL)
		{
			//TODO: fallback to SW
			Logger::getInstance().LogError("Direct 3D device manager is missing fall back to SW.");
			bRet = false;
			break;
		}

		if( !(m_hw_device_ctx = av_hwdevice_ctx_alloc(type)))
		{
			Logger::getInstance().LogError("Failed to alloc specified HW device.");
	    	bRet = false;
	    	break;
		}

		AVHWDeviceContext *ctx = (AVHWDeviceContext *)m_hw_device_ctx->data;
		AVDXVA2DeviceContext *hwctx = (AVDXVA2DeviceContext*)ctx->hwctx;

		hwctx->devmgr = m_deviceManager9;

		if (av_hwdevice_ctx_init(m_hw_device_ctx) < 0)
		{
			Logger::getInstance().LogError("Failed to init specified HW device.");
	    	bRet = false;
	    	break;
		}

	    m_avContext->hw_device_ctx = av_buffer_ref(m_hw_device_ctx);

		if ((avcodec_open2(m_avContext, m_avCodec, NULL)) < 0) {
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
	}
	while (false);

	return bRet;
}

bool hw_decoder_impl::release()
{
	if(m_hw_device_ctx != NULL){
		av_buffer_unref(&m_hw_device_ctx);
		m_hw_device_ctx = NULL;
	}

	return abs_decoder_impl::release();
}

bool hw_decoder_impl::decode(unsigned char* in, int in_size, void*& surface, int none)
{
	//Initialization check
	if(m_avPkt == NULL || m_avContext == NULL || m_avFrame == NULL || m_hw_device_ctx == NULL)
	{
		Logger::getInstance().LogError("HW mode decoder not initialized");
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
			Logger::getInstance().LogWarn("Error during decoding (avcodec_send_packet)");
			break;
		}

        ret = avcodec_receive_frame(m_avContext, m_avFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){	
            break;
		}
        else if (ret < 0) {
			Logger::getInstance().LogWarn("Error while decoding (avcodec_receive_frame)");
			break;
        }

		if (m_avFrame->format != m_hw_pix_fmt) {
			Logger::getInstance().LogWarn("Decoded frame is SW instead of HW!");
			bRet = false;
			break;
		}

		if(m_avFrame->data[3] != NULL)
		{
			surface = m_avFrame->data[3];
		}
		else
		{
			Logger::getInstance().LogWarn("Surface is not ready with decoded frame");
			break;
		}
	}
	while (false);

	return bRet;
}

AVPixelFormat hw_decoder_impl::get_hw_format(AVCodecContext* ctx, const AVPixelFormat* pix_fmts)
{
	hw_decoder_impl* this_pointer = static_cast<hw_decoder_impl*>(ctx->opaque);
    return this_pointer->get_hw_format_internal(ctx, pix_fmts);
}

AVPixelFormat hw_decoder_impl::get_hw_format_internal(AVCodecContext* ctx, const AVPixelFormat* pix_fmts)
{
	if(ctx->profile == FF_PROFILE_H264_BASELINE)
		ctx->hwaccel_flags |= AV_HWACCEL_FLAG_ALLOW_PROFILE_MISMATCH;

	const enum AVPixelFormat *p;
    for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
        if (*p == m_hw_pix_fmt) {
			init_hwaccel(ctx, *p);
	        return *p;
        }    	
    }
    Logger::getInstance().LogWarn("Failed to get HW surface format.");
    
	/* Fallback to default behaviour */
    return avcodec_default_get_format( ctx, pix_fmts );
}

int hw_decoder_impl::init_hwaccel(AVCodecContext* ctx, enum AVPixelFormat hw_pix_fmt)
{
	AVHWFramesContext *hw_frame_ctx = NULL;

    do
    {
    	if (!ctx->hw_device_ctx) {
	        Logger::getInstance().LogError("Missing device context");
	        break;
	    }

		if(ctx->hw_frames_ctx != NULL) {
			av_buffer_unref(&ctx->hw_frames_ctx);
		}

	    if (avcodec_get_hw_frames_parameters(ctx, ctx->hw_device_ctx, hw_pix_fmt, &ctx->hw_frames_ctx) < 0) {
	    	Logger::getInstance().LogError("Hardware decoding of this stream is unsupported?");
	    	break;
	    }

	    hw_frame_ctx = (AVHWFramesContext*)ctx->hw_frames_ctx->data;

	    // 17 surface is already included by libavcodec. The field is 0 if the
	    // hwaccel supports dynamic surface allocation.
	    if (hw_frame_ctx->initial_pool_size)
	        hw_frame_ctx->initial_pool_size = m_numOfSurfaces;  

	    if (av_hwframe_ctx_init(ctx->hw_frames_ctx) < 0) {
	        Logger::getInstance().LogError("Failed to allocate hw frames.");
	        break;
	    }

	    return 0;
    }
    while (false);

	return -1;
}