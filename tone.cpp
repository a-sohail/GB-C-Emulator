#include "tone.hpp"

void Tone::trigger(){
    
    enabled = true;
    if (soundLength == 0) {
        soundLength = 64;
    }
    
    frequency = (2048 - frequencyRegister) * 4;
    envelopeRunning = true;
    stepLength = numEnvelopeSweep;
    volume = volumeEnvelope;
    
    sweepFrequency = frequencyRegister;
    sweepPeriod = sweepTime;
    
    if(sweepPeriod == 0){
        sweepPeriod = 8;
    }
    sweepEnable = sweepPeriod > 0 || sweepShift > 0;
    
    if(sweepShift > 0){
        sweepCalculation();
    }
}

WORD Tone::sweepCalculation(){
    
    WORD newFrequency = 0;
    newFrequency = sweepFrequency >> sweepShift;
    
    if(sweepDecrease){
        newFrequency = sweepFrequency - newFrequency;
    }
    else{
        newFrequency = sweepFrequency + newFrequency;
    }
    
    if(newFrequency > 2047){
        enabled = false;
    }
    
    return newFrequency;
}

void Tone::writeByte(WORD address, BYTE val){
    
    switch ((address & 0xF) % 5) {
        case 0x0:
            sweepShift = val & 0x7;
            sweepDecrease = (val & 0x8) == 0x8;
            sweepTime = (val >> 4) & 0x7;
            break;
        case 0x1:
            duty = (val >> 6) & 0x3;
            soundLengthData = (val & 0x3F);
            break;
        case 0x2:
            digitalToAnalog = (val & 0xF8) != 0;
            volumeEnvelope = (val >> 4) & 0xF;
            volume = volumeEnvelope;
            increaseEnvelope = (val >> 3) & 0x1;
            numEnvelopeSweep = val & 0x7;
            stepLength = numEnvelopeSweep;
            break;
        case 0x3:
            frequencyRegister &= 0xFF00;
            frequencyRegister |= val;
            break;
        case 0x4:
            initial = (val >> 7) & 0x1;
            counterConsecutiveSelection = (val >> 6) & 0x1;
            frequencyRegister &= 0x00FF;
            frequencyRegister |= ((val & 0x7) << 8);
            if(initial){
                trigger();
            }
            break;
        default:
            break;
    }
}

BYTE Tone::readByte(WORD address){
    
    BYTE returnVal = 0x0;
    
    switch ((address & 0xF) % 0x5) {
        case 0x0:
            returnVal = (sweepTime << 4) | ((sweepDecrease) << 3) | (sweepShift);
            break;
        case 0x1:
            returnVal = ((duty & 0x3) << 6) | (soundLengthData & 0x3F);
            break;
        case 0x2:
            returnVal = ((volumeEnvelope & 0xF) << 4) | (increaseEnvelope << 3) | (numEnvelopeSweep & 0x7);
            break;
        case 0x3:
            returnVal = frequencyRegister & 0xFF;
            break;
        case 0x4:
            returnVal = (initial << 7) | (counterConsecutiveSelection << 6) | ((frequencyRegister >> 8) & 0x7);
            break;
        default:
            break;
    }
    return returnVal;
}

void Tone::adjustLength(){
    
    if(soundLength > 0 && counterConsecutiveSelection){
        soundLength--;
        if (soundLength == 0) {
            enabled = false;
        }
    }
}

void Tone::adjustSweep(){
    
    if(--sweepPeriod <= 0){
        
        sweepPeriod = sweepTime;
        
        if(sweepPeriod == 0){
            sweepPeriod = 8;
        }
        
        if(sweepEnable && sweepTime > 0){
            WORD newFrequency = sweepCalculation();
            if (newFrequency <= 2047 && sweepShift > 0) {
                sweepFrequency = newFrequency;
                frequencyRegister = newFrequency;
                sweepCalculation();
            }
            sweepCalculation();
        }
    }
}

void Tone::adjustEnvelope(){
    
    if(--stepLength <= 0){
        
        stepLength = numEnvelopeSweep;
        
        if(stepLength == 0){
            stepLength = 8;
        }
        
        if(envelopeRunning && numEnvelopeSweep > 0){
            if (increaseEnvelope && volume < 15) {
                volume++;
            }
            else if(!increaseEnvelope && volume > 0){
                volume--;
            }
        }
        
        if(volume == 0 || volume == 15){
            envelopeRunning = false;
        }
    }
}

void Tone::step(){
    
    if(--frequency <= 0){
        frequency = (2048 - frequencyRegister) * 4;
        waveDutyPointer = (waveDutyPointer + 1) & 0x7;
    }
    
    if(enabled && digitalToAnalog){
        outputVolume = volume;
    }
    else{
        outputVolume = 0;
    }
    
    if(!waveDuty[duty][waveDutyPointer]){
        outputVolume = 0;
    }
}

BYTE Tone::getOutputVolume(){
    return outputVolume;
}

bool Tone::isRunning(){
    return soundLength > 0;
}
