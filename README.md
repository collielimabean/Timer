# Timer
Simple timer that on expiration executes a callback.

I created this because my initial pure C++11 implementation hogged CPU time
as it just used a thread and checked if enough time had elapsed. This 
implementation wraps OS specific timer APIs in order to avoid effectively a
worker thread in a spin loop.

## Example

```C++
#include "Timer.h"
#include <ctime>
#include <iostream>

void Callback()
{
    time_t current = time(nullptr);
    std::cout << "Callback executed on " << std::ctime(&current) << std::endl;
}

int main()
{
    Timer t(Interval(1000), &Callback, true);
    t.Start();

    char x;
    std::cin >> x;

    return 0;
}
```