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
    double m_secondsPerCount;
    double m_deltaTime;

    __int64 m_BaseTime;
    __int64 m_PausedTime;
    __int64 m_StopTime;
    __int64 m_PrevTime;
    __int64 m_CurrTime;

    bool m_Stopped;
};

#endif // VKD3D12_BASE_TIMER_H
