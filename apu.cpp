#include "apu.hpp"
#include "definitions.hpp"

void APU::reset(){
    
    SDL_zero(audioSpec);
    audioSpec.freq = 44100;
    audioSpec.format = AUDIO_F32SYS;
    audioSpec.channels = 2;
    audioSpec.samples = SAMPLESIZE;
    audioSpec.callback = NULL;
    audioSpec.userdata = this;
    
    if(SDL_OpenAudio(&audioSpec, &obtainedSpec) < 0){
        SDL_Log("Did not get the audio format");
    }
    else{
        SDL_PauseAudio(0);
    }
}

void APU::writeByte(WORD address, BYTE val){
    if(address >= 0xFF10 && address <= 0xFF14){
        tone1.writeByte(address, val);
    }
    else if(address >= 0xFF16 && address <= 0xFF19){
        tone2.writeByte(address, val);
    }
    else if(address >= 0xFF24 && address <= 0xFF26){
        switch (address & 0xFF) {
            case 0x24:
                leftOutputLevel = (val >> 4) & 0x7;
                rightOutputLevel = val & 0x7;
                break;
            case 0x25:
                for(int i = 0; i < 8; i++){
                    if(i >= 4){
                        leftSoundEnable[i - 4] = (val >> i) & 0x1;
                    }
                    else{
                        rightSoundEnable[i] = (val >> i) & 0x1;
                    }
                }
                break;
            case 0x26:
                soundControl = (val >> 7) & 0x1;
                if(!soundControl){
                    for(int i = 0xFF10; i <= 0xFF25; i++){
                        writeByte(i, 0);
                    }
                }
                break;
            default:
                break;
        }
    }
}

BYTE APU::readByte(WORD address){
    
    BYTE returnValue = 0x0;
    
    if(address >= 0xFF10 && address <= 0xFF14){
        returnValue = tone1.readByte(address);
    }
    else if(address >= 0xFF16 && address <= 0xFF19){
        returnValue = tone2.readByte(address);
    }
    else if(address >= 0xFF24 && address <= 0xFF26){
        switch (address & 0xFF) {
            case 0x24:
                returnValue = (leftOutputLevel << 4) | (rightOutputLevel);
                break;
            case 0x25:
                for(int i = 0; i < 8; i++){
                    if(i < 4){
                        returnValue |= (rightSoundEnable[i] << i);
                    }
                    else{
                        returnValue |= (leftSoundEnable[i-4] << i);
                    }
                }
                break;
            case 0x26:
                returnValue = soundControl;
                returnValue <<= 7;
                returnValue |= ((tone2.isRunning() << 1) | tone1.isRunning());
                break;
            default:
                break;
        }
        
    }
    
    return returnValue;
    
}

void APU::step(int cycles){
    
    if(!soundControl){
        return;
    }
    
    while(cycles-- != 0){
        
        if(--clockCounter <= 0){
            
            clockCounter = 8192;
            
            switch(clockStep){
                case 0:
                    tone1.adjustLength();
                    tone2.adjustLength();
                    break;
                case 2:
                    tone1.adjustSweep();
                    tone1.adjustLength();
                    tone2.adjustLength();
                    break;
                case 4:
                    tone1.adjustLength();
                    tone2.adjustLength();
                    break;
                case 6:
                    tone1.adjustSweep();
                    tone1.adjustLength();
                    tone2.adjustLength();
                    break;
                case 7:
                    tone1.adjustEnvelope();
                    tone2.adjustEnvelope();
                    break;
            }
            
            clockStep++;
            
            if (clockStep >= 8) {
                clockStep = 0;
            }
        }
        
        tone1.step();
        tone2.step();
        
        if(--downSampleCount <= 0){
            downSampleCount = 95;
            
            float bufferIn0 = 0;
            float bufferIn1 = 0;
            
            int volume = (128 * leftOutputLevel) / 7;
            
            for(int i = 0; i < 4; i++){
                if(leftSoundEnable[i]){
                    switch (i) {
                        case 0:
                            bufferIn1 = ((float) tone1.getOutputVolume()) / 100;
                            break;
                        case 1:
                            bufferIn1 = ((float) tone2.getOutputVolume()) / 100;
                            break;
                        case 2:
                            break;
                        case 3:
                            break;
                        default:
                            break;
                    }
                    SDL_MixAudioFormat((Uint8*) &bufferIn0, (Uint8*) &bufferIn1, AUDIO_F32SYS, sizeof(float), volume);
                }
            }
            
            mainBuffer[bufferFillAmount++] = bufferIn0;
            
            bufferIn0 = 0;
            volume = (128 * rightOutputLevel) / 7;
            
            for(int i = 0; i < 4; i++){
                if(rightSoundEnable[i]){
                    switch (i) {
                        case 0:
                            bufferIn1 = ((float) tone1.getOutputVolume()) / 100;
                            break;
                        case 1:
                            bufferIn1 = ((float) tone2.getOutputVolume()) / 100;
                            break;
                        case 2:
                            break;
                        case 3:
                            break;
                        default:
                            break;
                    }
                    SDL_MixAudioFormat((Uint8*) &bufferIn0, (Uint8*) &bufferIn1, AUDIO_F32SYS, sizeof(float), volume);
                }
            }
            
            mainBuffer[bufferFillAmount++] = bufferIn0;
        }
        
        if (bufferFillAmount >= SAMPLESIZE) {
            bufferFillAmount = 0;
            while(SDL_GetQueuedAudioSize(1) > SAMPLESIZE * sizeof(float)){
                SDL_Delay(1);
            }
            SDL_QueueAudio(1, mainBuffer, SAMPLESIZE * sizeof(float));
        }
    }
}

APU apu;
