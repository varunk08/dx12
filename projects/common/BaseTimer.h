#pragma once
#ifndef VKD3D12_BASE_TIMER_H
#define VKD3D12_BASE_TIMER_H

class BaseTimer
{
public:
    BaseTimer();

    float TotalTimeInSecs() const;
    float DeltaTimeInSecs() const;

    void Reset();
    void Stop();
    void Start();
    void Tick();
private:
    // Time in secs
    double m_secondsPerCount;
    double m_deltaTime;

     // Time in counts obtained from QueryPerformanceCounter
    __int64 m_baseTime;
    __int64 m_pausedTime;
    __int64 m_stopTime;
    __int64 m_prevTime;
    __int64 m_currTime;

    bool m_stopped;
};

#endif // VKD3D12_BASE_TIMER_H
