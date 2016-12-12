#pragma once

#include <functional>
#include <chrono>

typedef std::chrono::milliseconds Interval;
typedef std::function<void(void)> TimerCallback;

class Timer
{
public:
    Timer(const Interval& timer_period, const TimerCallback& timer_callback, bool periodic = false);
    ~Timer();
    void Start();
    void Stop();
    Interval GetPeriod() const;
    void SetPeriod(const Interval& new_period);
    TimerCallback GetCallback() const;
    void SetCallback(const TimerCallback& new_callback);
    bool IsPeriodic() const;
    void SetPeriodic(bool periodic);
    bool IsRunning() const;

private:
    bool InitializeImpl();
    void CleanupImpl();

    Interval period;
    TimerCallback callback;
    bool isPeriodic;
    bool isRunning;

    struct TimerImpl;
    TimerImpl *impl;
};

