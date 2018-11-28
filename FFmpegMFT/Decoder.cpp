#include "stdafx.h"
#include "Decoder.h"


/*test*/


static int width, height;
static enum AVPixelFormat pix_fmt;

static FILE *video_dst_file = NULL;
static uint8_t *video_dst_data[4] = {NULL};
static int      video_dst_linesize[4];
static int video_dst_bufsize;

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
	bool bRet = true;

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


	return bRet;
}

bool Decoder::decode(unsigned char* in, int in_size, unsigned char* out, int* out_size)
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

		//ile (ret >= 0) {
	        ret = avcodec_receive_frame(m_avContext, m_avFrame);
	        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
				bRet = false;
	            break;
			}
	        else if (ret < 0) {
	            bRet = false;
				break;
	        }
		//	
	}
	while (false);

	if(bRet == true)
	{
		out = m_avFrame->data[0];
		if(out_size != NULL)
			*out_size = av_image_get_buffer_size(m_avContext->pix_fmt, m_avContext->width,m_avContext->height,1);
	}

	// /* allocate image where the decoded image will be put */
 //       width = m_avContext->width;
 //       height = m_avContext->height;
 //       pix_fmt = m_avContext->pix_fmt;
	//
 //       ret = av_image_alloc(video_dst_data, video_dst_linesize,
 //                            width, height, pix_fmt, 1);
 //       if (ret < 0) {
 //           fprintf(stderr, "Could not allocate raw video buffer\n");

 //       }
 //       video_dst_bufsize = ret;

	///* copy decoded frame to destination buffer:
	// *              * this is required since rawvideo expects non aligned data */
 //           av_image_copy(video_dst_data, video_dst_linesize,
 //                         (const uint8_t **)(m_avFrame->data), m_avFrame->linesize,
 //                         pix_fmt, width, height);
 //           /* write to rawvideo file */

	//	fopen_s(&video_dst_file,"c:\\asaf", "wb");
 //           fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);
	//fclose(video_dst_file);

	// av_free(video_dst_data[0]);

	return bRet;
}
