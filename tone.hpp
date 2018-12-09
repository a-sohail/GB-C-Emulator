#ifndef tone_hpp
#define tone_hpp

#include "definitions.hpp"

class Tone{
    
    BYTE duty = 0;
    BYTE soundLengthData = 0;
    float soundLength = 0;
    
    bool useSoundLength = false;
    
    bool waveDuty[4][8] = {
        {0, 1, 1, 1, 1, 1, 1, 1},
        {0, 0, 1, 1, 1, 1, 1, 1},
        {0, 0, 0, 0, 1, 1, 1, 1},
        {0, 0, 0, 0, 0, 0, 1, 1}
    };
    
    BYTE waveDutyPointer = 0;
    
    BYTE outputVolume = 0;
    BYTE volume = 0;
    BYTE volumeEnvelope = 0;
    bool increaseEnvelope = false;
    bool envelopeRunning = false;
    BYTE numEnvelopeSweep = 0;
    float stepLength = 0;
    
    WORD frequencyRegister = 0;
    bool counterConsecutiveSelection = false;
    bool initial = false;
    int frequency = 0;
    
    bool digitalToAnalog = false;
    bool enabled = false;
    
    BYTE sweepTime = 0;
    int sweepPeriod = 0;
    bool sweepDecrease = false;
    BYTE sweepShift = 0;
    WORD sweepFrequency = 0;
    bool sweepEnable = false;
    
    void trigger();
    WORD sweepCalculation();
    
public:
    
    void writeByte(WORD address, BYTE val);
    BYTE readByte(WORD address);
    void adjustLength();
    void adjustSweep();
    void adjustEnvelope();
    void step();
    BYTE getOutputVolume();
    bool isRunning();
};

#endif /* tone_hpp */
