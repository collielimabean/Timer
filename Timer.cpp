#include "Timer.h"
#include <stdexcept>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>

    struct Timer::TimerImpl
    {
        HANDLE timerQueue;
        HANDLE timerQueueTimer;
    };

#elif __linux__
    #include <unistd.h>
    #include <sys/timerfd.h>
    #include <sys/epoll.h>
    #include <string.h>
    #include <thread>

    struct Timer::TimerImpl
    {
        int timerfd;
        int epollfd;
        std::thread poller;
    };

#else
    #error "Not implemented"
#endif


Timer::Timer(const Interval& timer_period, const TimerCallback& timer_callback, bool periodic)
{
    this->period = timer_period;
    this->callback = timer_callback;
    this->isPeriodic = periodic;

    this->impl = new Timer::TimerImpl;
    if (!this->InitializeImpl())
        throw std::runtime_error("Failed to initialize timer!");
}

Timer::~Timer()
{
    this->isRunning = false;
    if (this->impl)
    {
        CleanupImpl();
        delete impl;
    }
}

Interval Timer::GetPeriod() const
{
    return this->period;
}

void Timer::SetPeriod(const Interval& new_period)
{
    this->period = new_period;
}

TimerCallback Timer::GetCallback() const
{
    return this->callback;
}

void Timer::SetCallback(const TimerCallback& new_callback)
{
    this->callback = new_callback;
}

bool Timer::IsPeriodic() const
{
    return this->isPeriodic;
}

void Timer::SetPeriodic(bool periodic)
{
    this->isPeriodic = periodic;
}

bool Timer::IsRunning() const
{
    return this->isRunning;
}

#ifdef _WIN32

static void CALLBACK WinTimerCallback(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
{
    if (!lpParameter)
        return;

    Timer *t = (Timer *) lpParameter;
    if (t->GetCallback())
        t->GetCallback()();
}

bool Timer::InitializeImpl()
{
    impl->timerQueue = CreateTimerQueue();
    return impl->timerQueue != INVALID_HANDLE_VALUE;
}

void Timer::CleanupImpl()
{
    if (impl->timerQueueTimer != INVALID_HANDLE_VALUE)
        DeleteTimerQueueTimer(impl->timerQueue, impl->timerQueueTimer, INVALID_HANDLE_VALUE);
    
    if (impl->timerQueue != INVALID_HANDLE_VALUE)
        DeleteTimerQueueEx(impl->timerQueue, INVALID_HANDLE_VALUE);
}

void Timer::Start()
{
    DWORD period = (this->isPeriodic) ? static_cast<DWORD>(this->GetPeriod().count()) : 0;

    BOOL success = CreateTimerQueueTimer(
        &impl->timerQueueTimer,
        impl->timerQueue,
        &WinTimerCallback,
        this,
        0,
        period,
        0
    );

    if (!success)
        throw std::runtime_error("Failed to start timer!");

    this->isRunning = true;
}

void Timer::Stop()
{
    this->isRunning = false;
    this->CleanupImpl();
    this->InitializeImpl();
}

#elif __linux__
constexpr int EPOLL_EVENT_SIZE = 1;

bool Timer::InitializeImpl()
{
    epoll_event event;

    impl->timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (impl->timerfd == -1)
        return false;

    impl->epollfd = epoll_create(EPOLL_EVENT_SIZE);
    if (impl->epollfd == -1)
    {
        close(impl->timerfd);
        return false;
    }

    event.events = EPOLLIN;
    event.data.fd = impl->timerfd;

    if (epoll_ctl(impl->epollfd, EPOLL_CTL_ADD, impl->timerfd, &event) == -1)
    {
        close(impl->epollfd);
        close(impl->timerfd);
        return false;
    }
    
    return true;
}

void Timer::CleanupImpl()
{
    close(impl->timerfd);
    close(impl->epollfd);
    impl->poller.join();
}

void Timer::Start()
{
    struct itimerspec interval;

    auto timeval = this->GetPeriod().count(); // millis

    // convert val to (seconds, nanoseconds)
    memset(&interval, 0, sizeof(struct itimerspec));
    interval.it_value.tv_sec = static_cast<int>(timeval) / 1000;
    interval.it_value.tv_nsec = 1000000 * (static_cast<int>(timeval) % 1000); 

    if (this->isPeriodic)
    {
        interval.it_interval.tv_sec = interval.it_value.tv_sec;
        interval.it_interval.tv_nsec = interval.it_value.tv_nsec;
    }

    if (timerfd_settime(impl->timerfd, 0, &interval, nullptr) == -1)
    {
        close(impl->timerfd);
        close(impl->epollfd);
        throw std::runtime_error("Failed to start timer!");
    }

    this->isRunning = true;

    // start poll thread
    impl->poller = std::thread([&]()
    {
        epoll_event events[EPOLL_EVENT_SIZE];

        while (this->isRunning)
        {
            int num__events = epoll_wait(impl->epollfd, events, EPOLL_EVENT_SIZE, 1);
            if (num__events == -1)
                continue;

            size_t s = 0;
            int i = read(impl->timerfd, &s, sizeof(size_t));
            if (i != -1 && this->GetCallback())
                this->GetCallback()();
        }
    });
}

void Timer::Stop()
{
    this->isRunning = false;
    if (impl->poller.joinable())
        impl->poller.join();
}

#endif
