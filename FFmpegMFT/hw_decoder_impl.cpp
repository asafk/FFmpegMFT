#include "stdafx.h"
#include "hw_decoder_impl.h"


hw_decoder_impl::hw_decoder_impl() : 
m_decoder_ctx(NULL),
m_decoder(NULL),
m_packet(NULL),
m_avFrame(NULL)
{
}


hw_decoder_impl::~hw_decoder_impl()
{
}

bool hw_decoder_impl::init(std::string codecName)
{
	bool bRet = true;
	do
	{
		m_packet = av_packet_alloc();
		if (m_packet == NULL){
			bRet = false;
			break;
		}

		int i;

		m_type = av_hwdevice_find_type_by_name("d3d11va"); //dxva2 failed
	    if (m_type == AV_HWDEVICE_TYPE_NONE) {
	        fprintf(stderr, "Device type %s is not supported.\n", "d3d11va");
	        fprintf(stderr, "Available device types:");
	        while((m_type = av_hwdevice_iterate_types(m_type)) != AV_HWDEVICE_TYPE_NONE)
	            fprintf(stderr, " %s", av_hwdevice_get_type_name(m_type));
	        fprintf(stderr, "\n");
			bRet = false;
	        break;
	    }

		/* find the video decoder according to codecName*/
		/* currently support only HEVC or H.264*/
		if(codecName.compare("HEVC") == 0)
			m_decoder = avcodec_find_decoder(AV_CODEC_ID_HEVC);
		else
			m_decoder = avcodec_find_decoder(AV_CODEC_ID_H264);

	    if (m_decoder == NULL) {
			bRet = false;
			break;
	    }

		for (i = 0;; i++) {
	        const AVCodecHWConfig *config = avcodec_get_hw_config(m_decoder, i);
	        if (!config) {
	            fprintf(stderr, "Decoder %s does not support device type %s.\n",
	                    m_decoder->name, av_hwdevice_get_type_name(m_type));
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

		if (!(m_decoder_ctx = avcodec_alloc_context3(m_decoder))){
			 bRet = false;
			 break;
		}

		m_decoder_ctx->opaque = this;
		m_decoder_ctx->get_format = get_hw_format;
		m_decoder_ctx->get_buffer2 = get_buffer;

	    if ((av_hwdevice_ctx_create(&m_hw_device_ctx, m_type,
	                                      NULL, NULL, 0)) < 0) {
	    	fprintf(stderr, "Failed to create specified HW device.\n");
	    	bRet = false;
	    	break;
	    }
	    m_decoder_ctx->hw_device_ctx = av_buffer_ref(m_hw_device_ctx);

		if ((avcodec_open2(m_decoder_ctx, m_decoder, NULL)) < 0) {
	        fprintf(stderr, "Failed to open codec for stream \n");
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

bool hw_decoder_impl::release()
{
	bool bRet = true;
	do
	{
		if(m_packet != NULL){
			av_packet_free(&m_packet);
			m_packet = NULL;
		}

		if(m_avFrame != NULL){
			av_frame_free(&m_avFrame);
			m_avFrame = NULL;
		}

		if(m_decoder_ctx != NULL){
			avcodec_free_context(&m_decoder_ctx);
			m_decoder_ctx = NULL;
		}

		if(m_hw_device_ctx != NULL){
			av_buffer_unref(&m_hw_device_ctx);
			m_hw_device_ctx = NULL;
		}
	}
	while (false);

	return bRet;
}

void hw_decoder_impl::flush()
{
	AVFrame *frame = NULL;
	do
	{
		if (!(frame = av_frame_alloc())) {
			fprintf(stderr, "Can not alloc frame\n");
			break;
		}

		int ret = avcodec_send_packet(m_decoder_ctx, NULL);
		if (ret < 0) {
		    break;
		}
		while (ret >= 0) 
		{
		    ret = avcodec_receive_frame(m_decoder_ctx, frame);
		    if (ret == AVERROR_EOF)
		        break;
		}

		av_frame_free(&frame);
	} while (false);

	avcodec_flush_buffers(m_decoder_ctx);
}	

bool hw_decoder_impl::decode(unsigned char* in, int in_size, void*& surface, int none)
{
	bool bRet = false;
	AVFrame *frame = NULL;
    int ret = 0;

	do
	{
		m_packet->data = in;
		m_packet->size = in_size;
		m_surface = (LPDIRECT3DSURFACE9)surface;

		ret = avcodec_send_packet(m_decoder_ctx, m_packet);
		if (ret < 0) {
			bRet = false;
			break;
		}

        ret = avcodec_receive_frame(m_decoder_ctx, m_avFrame);
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

	return bRet;
}

AVPixelFormat hw_decoder_impl::get_hw_format(AVCodecContext* ctx, const AVPixelFormat* pix_fmts)
{
	hw_decoder_impl* this_pointer = static_cast<hw_decoder_impl*>(ctx->opaque);
    return this_pointer->get_hw_format_internal(ctx, pix_fmts);
}

AVPixelFormat hw_decoder_impl::get_hw_format_internal(AVCodecContext* ctx, const AVPixelFormat* pix_fmts)
{
	const enum AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++) {
        if (*p == m_hw_pix_fmt)
            return *p;
    }
    fprintf(stderr, "Failed to get HW surface format.\n");
    
	/* Fallback to default behaviour */
    return avcodec_default_get_format( ctx, pix_fmts );
}

int hw_decoder_impl::get_buffer(struct AVCodecContext *c, AVFrame *frame, int flags)
{
	hw_decoder_impl* this_pointer = static_cast<hw_decoder_impl*>(c->opaque);
    return this_pointer->get_buffer_internal(c, frame, flags);
}

int hw_decoder_impl::get_buffer_internal(struct AVCodecContext *c, AVFrame *pic, int flags)
{
	 /* */
    for (int i = 0; i < 4; i++) {
        pic->data[i] = NULL;
        pic->linesize[i] = 0;

        if (i == 0 || i == 3)
            pic->data[i] = (uint8_t *)m_surface;/* Yummie */
    }

	return 1;
	/* Not much to do in indirect rendering mode. */
    return avcodec_default_get_buffer2( c, pic, flags );
}
