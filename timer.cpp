#include "timer.hpp"
#include "registers.hpp"

void Timer::addToClock(int clockCycles){
    
    //clockCycles /= 4;
    
    // handle dividers
    dividerClock += clockCycles;
    if(dividerClock >= 256){
        dividerClock %= 256;
        divider++;
    }
    
    if(isClockEnabled){
        
        controlClock -= clockCycles;
        
        if(controlClock <= 0){
            
            setControlRate();
            
            if(counter == 0xFF){
                counter = modulo;
                // Request Timer Interrupt by setting bit 2 in IF Register
                ifRegister |= 0x4;
            }
            else{
                counter++;
            }
        }
    }
}

void Timer::setControlRate(){
    switch(control & 0x3){
        case 0:
            controlClock = (CLOCKSPEED / 4096);
            break;
        case 1:
            controlClock = (CLOCKSPEED / 262144);
            break;
        case 2:
            controlClock = (CLOCKSPEED / 65536);
            break;
        case 3:
            controlClock = (CLOCKSPEED / 16384);
            break;
        default:
            break;
    }
    isClockEnabled = (control & 0x4);
}

Timer timer;
