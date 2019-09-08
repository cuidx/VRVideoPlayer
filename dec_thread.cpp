#include "dec_thread.h"
#include "ren_thread.h"
#include "media_track.h"
#include "r_util.h"
#include "aren_thread.h"

extern "C"
{
#include "libavformat/avformat.h"
#include "libswscale\swscale.h"
};

void FFMPEG_Callback(void* ptr, int level, const char* fmt, va_list vl)
{
	// 可以根据level的级别选择要打印显示的内容
	//if (level <= AV_LOG_INFO)
	{
		char buffer[1024];
		vsprintf(buffer, fmt, vl);

		r_log("msg : [%d] %s", level, buffer);
	}
}

dec_thread::dec_thread()
    : m_head(NULL)
    , m_arthread(NULL)
    , m_vrthread(NULL)
    , m_vdec_ctx(NULL)
    , m_adec_ctx(NULL)
	, m_fmt_ctx(NULL)
{
}

dec_thread::~dec_thread()
{
    this->ClearQue();
}

AVPacket* dec_thread::Deque()
{
    AVPacket* pkt = NULL;

    while (true)
    {
        RLock();

        if(m_exit_flag)
        {
            this->ClearQue();
            RUnlock();
            return NULL;
        }

        if(m_head)
        {
            pkt = m_head->m_packet;

            if(m_head->m_next)
            {
                packet_queue* temp = m_head;
                m_head = m_head->m_next;
                free(temp);
            }
            else
            {
                free(m_head);
                m_head = NULL;
            }

            RUnlock();
            return pkt;
        }
        else
        {
            RUnlock();
            Sleep(10);
        }
    }
}

int dec_thread::Run()
{
    av_register_all();
	av_log_set_callback(&FFMPEG_Callback);

	if (NULL == m_fmt_ctx)
	{
		m_fmt_ctx = NULL;// avformat_alloc_context();
		int ret = avformat_open_input(&m_fmt_ctx, m_url.c_str(), NULL, NULL);
		if (ret != 0)
		{
			r_log("Failed to open input file: %s\n", m_url.c_str());
			return -1;
		}
		int i;
		int videoStream = -1;
		int audioStream = -1;
		//=================================== 获取视频流信息 ===================================//
		if (avformat_find_stream_info(m_fmt_ctx, NULL) < 0)
		{
			r_log("Could't find stream infomation.");
			return -1;
		}

		if (m_atrack == NULL)
		{
			m_atrack = new media_track();
		}

		float fAudioWaitTime = 0;
		int videoWaitTime = 0;

		for (i = 0; i < m_fmt_ctx->nb_streams; i++)
		{
			if (m_fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
			{
				videoStream = i;

				r_log("\nvideo stream duration = %lld\n", m_fmt_ctx->streams[i]->duration);
				r_log("video stream samle count = %lld\n", m_fmt_ctx->streams[i]->nb_frames);
				r_log("video stream av_frame_rate = %d / %d\n", m_fmt_ctx->streams[i]->avg_frame_rate.num, m_fmt_ctx->streams[i]->avg_frame_rate.den);
				videoWaitTime = m_fmt_ctx->streams[i]->avg_frame_rate.num / m_fmt_ctx->streams[i]->avg_frame_rate.den;
			}
			else if (m_fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
			{
				audioStream = i;

				r_log("\naudio stream duration = %lld\n",m_fmt_ctx->streams[i]->duration);
				r_log("audio stream samle count = %lld\n", m_fmt_ctx->streams[i]->nb_frames);
				//r_log("audio stream av_frame_rate = %d / %d\n", m_fmt_ctx->streams[i]->avg_frame_rate.num,m_fmt_ctx->streams[i]->avg_frame_rate.den);
				r_log("audio codec context framesize = %d\n", m_fmt_ctx->streams[i]->codec->frame_size);
				r_log("audio codec context sample rate = %d\n", m_fmt_ctx->streams[i]->codec->sample_rate);
				r_log("audio codec context channels = %d\n", m_fmt_ctx->streams[i]->codec->channels);
				fAudioWaitTime = 1000 / (m_fmt_ctx->streams[i]->codec->sample_rate * 1.0 / m_fmt_ctx->streams[i]->codec->frame_size);
				r_log("audio frame wait time = %f\n", fAudioWaitTime);
			}
		}

		if (0 != videoWaitTime)
		{
			videoWaitTime = 1000 / videoWaitTime;
		}

		r_log("video stream wait time = %d\n", videoWaitTime);

		//如果videoStream为-1 说明没有找到视频流
		if (videoStream == -1)
		{
			r_log("Didn't find a video stream.");
			return -1;
		}

		m_vdec_ctx = m_fmt_ctx->streams[videoStream]->codec;
		m_adec_ctx = m_fmt_ctx->streams[audioStream]->codec;

		AVCodec* pCodec = avcodec_find_decoder(m_vdec_ctx->codec_id);
		if (pCodec == NULL)
		{
			r_log("Codec not found.");
			return -1;
		}
		int nret = avcodec_open2(m_vdec_ctx, pCodec, NULL);
		if (nret < 0) {
			r_log("Could not open codec,err=%d\n", nret);
			return -1;
		}

		AVCodec* pAudioCodec = avcodec_find_decoder(m_adec_ctx->codec_id);
		if (pAudioCodec == NULL)
		{
			r_log("Audio Codec not found.");
			return -1;
		}
		nret = avcodec_open2(m_adec_ctx, pAudioCodec, NULL);
		if (nret < 0) {
			r_log("Could not open audio codec,err=%d\n", nret);
			return -1;
		}

		m_atrack->channel = m_adec_ctx->channels;
		m_atrack->sample = m_adec_ctx->sample_rate;
		m_atrack->is_aac = (m_adec_ctx->codec_id == AV_CODEC_ID_AAC);
		((aren_thread*)m_arthread)->SetParam(m_atrack);
		m_arthread->SetPacketPlayDuration(/*(int)fAudioWaitTime*/20);
		m_vrthread->SetPacketPlayDuration(videoWaitTime);

		AVPacket* packet = (AVPacket *)av_malloc(sizeof(AVPacket));
		while (av_read_frame(m_fmt_ctx, packet) >= 0)
		{
			if (packet->stream_index == videoStream)
			{
				//m_mutex.try_lock_for(std::chrono::milliseconds(videoWaitTime));
				this->DecodeVideo(packet);
			}
			else if (audioStream == packet->stream_index)
			{
				this->DecodeAudio(packet);
				m_mutex.try_lock_for(std::chrono::milliseconds((int)fAudioWaitTime));
			}
			//std::this_thread::sleep_for(std::chrono::milliseconds(30));

			av_packet_unref(packet);
			av_free_packet(packet);
		}
	}

    AVPacket* pkt = this->Deque();

    while(pkt)
    {
        if(pkt->stream_index == 0)
        {
            this->DecodeVideo(pkt);
        } 
        else if(pkt->stream_index == 1)
        {
            this->DecodeAudio(pkt);
        }        

        av_packet_unref(pkt);
		av_free_packet(pkt);
		delete pkt;
        pkt = this->Deque();
    }

    if(m_vdec_ctx)
    {
        //avcodec_free_context(&m_vdec_ctx);
    }
    if(m_adec_ctx)
    {
        //avcodec_free_context(&m_adec_ctx);
    }

    return 0;
}

void dec_thread::Enque(AVPacket* pkt)
{
    RLock();

    if (m_exit_flag)
    {
        RUnlock();
        av_packet_unref(pkt);
		av_free_packet(pkt);
		delete pkt;
        return;
    }

    if (!m_head) {
        packet_queue* pp = (packet_queue*)malloc(sizeof(packet_queue));
        pp->m_packet = pkt;
        pp->m_next = NULL;

        m_head = pp;
    }
    else
    {
        packet_queue* p = m_head;
        while(p->m_next) {
            p = p->m_next;
        }

        packet_queue* pp = (packet_queue*)malloc(sizeof(packet_queue));
        pp->m_packet = pkt;
        pp->m_next = NULL;
        p->m_next = pp;
    }

    RUnlock();
}

void dec_thread::ClearQue()
{
    while (m_head)
    {
        packet_queue* p = m_head;
        m_head = m_head->m_next;
        av_packet_unref(p->m_packet);
		av_free_packet(p->m_packet);
        free(p);
    }
}

int dec_thread::DecodeVideo(AVPacket* pkt)
{
	
    if(m_vdec_ctx == NULL)
    {
		return -1;
    }    

	AVFrame	*pFrameYUV = NULL;
	uint8_t *out_buffer = NULL;
	static struct SwsContext *img_convert_ctx = NULL;
	if (NULL == img_convert_ctx && m_vdec_ctx)
	{
		img_convert_ctx = sws_getContext(m_vdec_ctx->width, m_vdec_ctx->height, m_vdec_ctx->pix_fmt,
			m_vdec_ctx->width, m_vdec_ctx->height, PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
	}

	pFrameYUV = av_frame_alloc();
	out_buffer = (uint8_t *)av_malloc(avpicture_get_size(PIX_FMT_YUV420P, m_vdec_ctx->width, m_vdec_ctx->height));
	if (out_buffer == NULL)
	{
		r_log("av_malloc fail,width=%d,height=%d,buf=%p\n", m_vdec_ctx->width, m_vdec_ctx->height, out_buffer);
		av_frame_unref(pFrameYUV);
		av_frame_free(&pFrameYUV);
		exit(1);
		return -1;
	}
	int nret = avpicture_fill((AVPicture *)pFrameYUV, out_buffer, PIX_FMT_YUV420P, m_vdec_ctx->width, m_vdec_ctx->height);
	if (nret < 0)
	{
		r_log("avpicture_fill fail ret=%d,width=%d,height=%d,buf=%p\n",nret, m_vdec_ctx->width, m_vdec_ctx->height, out_buffer);
		av_frame_unref(pFrameYUV);
		av_frame_free(&pFrameYUV);
		av_free(out_buffer);
		return -1;
	}
	pFrameYUV->width = m_vdec_ctx->width;
	pFrameYUV->height = m_vdec_ctx->height;

	int got_picture = 0;
	AVFrame* frame = av_frame_alloc();
	if (!frame) {
		r_log("Could not allocate video frame\n");
		av_frame_unref(pFrameYUV);
		av_frame_free(&pFrameYUV);
		av_free(out_buffer);
		return -1;
	}
	int ret = avcodec_decode_video2(m_vdec_ctx, frame, &got_picture, pkt);
	if (ret < 0) {
		r_log("Decode Error.\n");
		av_frame_unref(frame);
		av_frame_free(&frame);
		av_frame_unref(pFrameYUV);
		av_frame_free(&pFrameYUV);
		av_free(out_buffer);
		return -1;
	}
	if (got_picture) {
		nret = sws_scale(img_convert_ctx, (const uint8_t* const*)frame->data, frame->linesize, 0, m_vdec_ctx->height,
			pFrameYUV->data, pFrameYUV->linesize);

		if (nret <= 0)
		{
			r_log("sws_scale fail ret=%d,width=%d,height=%d,buf=%p,pFrameYUV->linesize=%d,pFrameYUV->data=%p\n", 
				nret, m_vdec_ctx->width, m_vdec_ctx->height, out_buffer, pFrameYUV->linesize, pFrameYUV->data);
			av_frame_unref(frame);
			av_frame_free(&frame);
			av_frame_unref(pFrameYUV);
			av_frame_free(&pFrameYUV);
			av_free(out_buffer);
			return -1;
		}
		m_vrthread->Enque(pFrameYUV);
	}
   
    return 0;
}

int dec_thread::DecodeAudio(AVPacket* pkt)
{
    if(m_adec_ctx == NULL)
    {
		return -1;
    }

	AVFrame* frame = av_frame_alloc();
	if (!frame) {
		r_log("Could not allocate audio frame\n");
		return -1;
	}

	int get_frame = 0;
	int ret = avcodec_decode_audio4(m_adec_ctx, frame, &get_frame, pkt);
	if (get_frame != 0)
	{
		m_arthread->Enque(frame);
	}
	else
	{
		av_frame_unref(frame);
		av_frame_free(&frame);
	}

    //int ret = avcodec_send_packet(m_adec_ctx, pkt);
    //if (ret < 0) {
    //    r_log("Error submitting the packet to the decoder\n");
    //    return -1;
    //}

    ///* read all the output frames (in general there may be any number of them */
    //while (ret >= 0) {
    //    AVFrame* frame = av_frame_alloc();
    //    if (!frame) {
    //        r_log("Could not allocate video frame\n");
    //        return -1;
    //    }

    //    ret = avcodec_receive_frame(m_adec_ctx, frame);
    //    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
    //    {
    //        av_frame_unref(frame);
    //        av_frame_free(&frame);
    //        break;
    //    }
    //    else if (ret < 0) {
    //        r_log("Error during decoding\n");
    //        av_frame_unref(frame);
    //        av_frame_free(&frame);
    //        return -1;
    //    }

    //    m_arthread->Enque(frame);
    //}

    return 0;
}

int dec_thread::GetAudioParam(AVCodecID& codec_id, int& sample, int& channel)
{
	if (m_atrack)
	{
		sample = m_atrack->sample;
		channel = m_atrack->channel;

		return 0;
	}	

    return -1;
}
