#ifndef __REN_THREAD_H__
#define __REN_THREAD_H__

#include "r_thread.h"
#include <mutex>

struct AVFrame;

class ren_thread : public r_thread
{
private:
    class frame_queue
    {
    public:
        frame_queue()
            : m_frame(NULL)
            , m_next(NULL)
        {
        }

        AVFrame* m_frame;
        frame_queue* m_next;
    };

public:
    ren_thread();
    virtual ~ren_thread();

    void Enque(AVFrame* frame);
    AVFrame* Deque();
    int Run();
	void SetPacketPlayDuration(int duration)
	{
		m_playDuration = duration;
	}

private:
    void ClearQue();
    virtual int Render(AVFrame* frame) = 0;
    virtual void DestroyRender() = 0;

private:
    frame_queue* m_head;
	int m_queLen;
	int m_playDuration;
	std::timed_mutex m_mutex;
};

#endif