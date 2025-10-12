#pragma once

#include <chrono>
#include <thread>

using namespace std::chrono;

class FPSTimer
{
    using Clock = std::conditional_t<
        high_resolution_clock::is_steady, high_resolution_clock,
        std::conditional_t<
            system_clock::period::den <= steady_clock::period::den, system_clock, steady_clock>>;

    Clock::time_point m_second_start;
    Clock::time_point m_last_frame_end;
    size_t m_frames_this_second = 0;
    float m_fps = 0.;

  public:
    FPSTimer() : m_second_start(Clock::now()), m_last_frame_end(m_second_start) {}

    void frame_end()
    {
        m_last_frame_end = Clock::now();

        if (duration_cast<microseconds>(m_last_frame_end - m_second_start).count() >= long(1e6))
        {
            m_fps = static_cast<float>(m_frames_this_second);
            m_frames_this_second = 0;
            m_second_start = m_last_frame_end;
        }
        else
        {
            m_frames_this_second++;
        }
    }

    inline float current_fps()
    {
        return m_fps;
    }

    inline void wait_for_frame_duration(int request_us)
    {
        std::this_thread::sleep_until(m_last_frame_end + microseconds(request_us));
    }
};
