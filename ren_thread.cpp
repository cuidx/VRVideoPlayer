#include "ren_thread.h"
#include "r_util.h"

extern "C"
{
#include "libavformat/avformat.h"
};

ren_thread::ren_thread()
    : m_head(NULL), m_queLen(0)
{
}

ren_thread::~ren_thread()
{
    this->ClearQue();
}

AVFrame* ren_thread::Deque()
{
    AVFrame* pkt = NULL;

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
            pkt = m_head->m_frame;

            if(m_head->m_next)
            {
                frame_queue* temp = m_head;
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

int ren_thread::Run()
{
    AVFrame* frame = this->Deque();

    while(frame)
    {
        this->Render(frame);
        av_frame_unref(frame);
        av_frame_free(&frame);
        frame = this->Deque();
		m_mutex.try_lock_for(std::chrono::milliseconds(m_playDuration));
    }
    this->DestroyRender();

    return 0;
}

void ren_thread::ClearQue()
{
    while (m_head)
    {
        frame_queue* p = m_head;
        m_head = m_head->m_next;
        av_frame_unref(p->m_frame);
        av_frame_free(&p->m_frame);
        free(p);
    }
}

void ren_thread::Enque(AVFrame* frame)
{
    RLock();

    if(m_exit_flag)
    {
        RUnlock();
        av_frame_unref(frame);
        av_frame_free(&frame);
        return;
    }

    if (!m_head) {
        frame_queue* pp = (frame_queue*)malloc(sizeof(frame_queue));
        pp->m_frame = frame;
        pp->m_next = NULL;
        m_head = pp;
		m_queLen = 1;
    }
    else
    {
		m_queLen = 1;
        frame_queue* p = m_head;
        while(p->m_next) {
            p = p->m_next;
			m_queLen++;
        }

        frame_queue* pp = (frame_queue*)malloc(sizeof(frame_queue));
        pp->m_frame = frame;
        pp->m_next = NULL;
        p->m_next = pp;
		m_queLen += 1;

		r_log("ren_thread::Enque queue length = %d, duration=%d\n", m_queLen, m_playDuration);
    }

    RUnlock();
}
