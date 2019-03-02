#include <windows.h>
#include "BaseTimer.h"

BaseTimer::BaseTimer()
    :
    m_secondsPerCount(0.0),
    m_deltaTime(-1.0),
    m_baseTime(0),
    m_pausedTime(0),
    m_prevTime(0),
    m_currTime(0),
    m_stopped(false)
{
    __int64 countsPerSec;
    QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
    m_secondsPerCount = 1.0 / (double)countsPerSec;
}

float BaseTimer::TotalTimeInSecs() const
{
}

float BaseTimer::DeltaTimeInSecs() const{}

void BaseTimer::Reset()
{
    __int64 currTime = 0;
    QueryPerformanceCounter((LARGE_INTEGER*)(&currTime));

    m_baseTime = currTime;
    m_prevTime = currTime;
    m_stopTime = 0;
    m_stopped  = false;
}
void BaseTimer::Stop()
{
    if (m_stopped == false)
    {
        __int64 currTime = 0;
        QueryPerformanceCounter((LARGE_INTEGER*)(&currTime));

        m_stopTime = currTime;
        m_stopped = true;
    }
}
void BaseTimer::Start()
{
    if (m_stopped)
    {
        __int64 startTime;
        QueryPerformanceCounter((LARGE_INTEGER*)&startTime);

        m_pausedTime += startTime - m_stopTime;
        m_prevTime   = startTime;
        m_stopTime   = 0;
        m_stopped    = false;
    }
}

void BaseTimer::Tick()
{
    if (m_stopped)
    {
        m_deltaTime = 0.0;
    }

    __int64 currTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
    m_currTime = currTime;

    m_deltaTime = (m_currTime - m_prevTime) * m_secondsPerCount;
    m_prevTime = m_currTime;

    if (m_deltaTime < 0.0)
    {
        m_deltaTime = 0.0;
    }
}