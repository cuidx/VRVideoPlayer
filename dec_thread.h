#ifndef __DEC_THREAD_H__
#define __DEC_THREAD_H__

#include "r_thread.h"
#include "r_string.h"
#include <mutex>

struct AVPacket;
class ren_thread;
struct AVCodecContext;
struct AVFormatContext;
class media_track;
enum AVCodecID;

class dec_thread : public r_thread
{
private:
    class packet_queue
    {
    public:
        packet_queue()
            : m_packet(NULL)
            , m_next(NULL)
        {
        }

        AVPacket* m_packet;
        packet_queue* m_next;
    };

public:
    dec_thread();
    ~dec_thread();

    void Enque(AVPacket* pkt);
    AVPacket* Deque();
    int Run();
    inline void SetParam(ren_thread* vrthr, ren_thread* arthr, media_track* atrack, r_string& url)
    {
        m_vrthread = vrthr;
        m_arthread = arthr;
        m_atrack = atrack;
		m_url = url;
    }

private:
    void ClearQue();
    int DecodeVideo(AVPacket* pkt);
    int DecodeAudio(AVPacket* pkt);
    int GetAudioParam(AVCodecID& codec_id, int& sample, int& channel);

private:
    packet_queue* m_head;
    ren_thread * m_vrthread;
    ren_thread * m_arthread;
    media_track* m_atrack;
    AVCodecContext* m_vdec_ctx;
    AVCodecContext* m_adec_ctx;
	AVFormatContext* m_fmt_ctx;
	r_string m_url;
	std::timed_mutex m_mutex;
};

#endif