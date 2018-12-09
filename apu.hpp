#ifndef apu_hpp
#define apu_hpp

#include <SDL2/SDL.h>
#include "tone.hpp"

// Sample size for Audio
#define SAMPLESIZE 4096

class APU{
    
    BYTE leftOutputLevel = 0;
    BYTE rightOutputLevel = 0;
    
    bool leftSoundEnable[4] = {0, 0, 0, 0};
    bool rightSoundEnable[4] = {0, 0, 0, 0};
    
    bool soundControl = false;
    
    Tone tone1;
    Tone tone2;
    
    SDL_AudioSpec audioSpec;
    SDL_AudioSpec obtainedSpec;
    
    int downSampleCount = 95;
    int bufferFillAmount = 0;
    float mainBuffer[4096] = { 0 };
    
    int clockCounter = 8192;
    BYTE clockStep = 0;
    
public:
    
    void reset();
    void writeByte(WORD address, BYTE val);
    BYTE readByte(WORD address);
    void step(int cycles);
};

extern APU apu;

#endif /* apu_hpp */
