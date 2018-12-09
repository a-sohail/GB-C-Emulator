#ifndef timer_hpp
#define timer_hpp

#include "definitions.hpp"

class Timer{
    
public:
    
    BYTE divider = 0;
    BYTE counter = 0;
    BYTE modulo = 0;
    BYTE control = 0;
    
    int controlClock = 1024;
    int dividerClock = 0;
    
    bool isClockEnabled = true;
    void addToClock(int clockCycles);
    void setControlRate();
};

extern Timer timer;

#endif /* timer_hpp */
