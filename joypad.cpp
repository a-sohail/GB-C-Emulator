#include "joypad.hpp"
#include "registers.hpp"

BYTE JOYPAD::readByte(){
    switch (column) {
        case 0x10:
            return controls[1];
        case 0x20:
            return controls[0];
        default:
            return 0x0;
    }
}

void JOYPAD::writeByte(BYTE val){
    // Only care about bits 4 and 5
    column = val & 0x30;
}

void JOYPAD::reset(){
    controls[0] = 0x0F;
    controls[1] = 0x0F;
    column = 0x0;
}

void JOYPAD::keyDown(SDL_Keycode key){
    switch(key){
            // Up
        case SDLK_w:
            controls[0] &= 0x0B;
            break;
            // Left
        case SDLK_a:
            controls[0] &= 0x0D;
            break;
            // Down
        case SDLK_s:
            controls[0] &= 0x07;
            break;
            // Right
        case SDLK_d:
            controls[0] &= 0x0E;
            break;
            // A
        case SDLK_z:
            controls[1] &= 0x0E;
            break;
            // B
        case SDLK_x:
            controls[1] &= 0x0D;
            break;
            // Start
        case SDLK_LSHIFT:
            controls[1] &= 0x07;
            break;
            // Select
        case SDLK_SPACE:
            controls[1] &= 0x0B;
            break;
        default: break;
    }
    
    // Joypad Interrupt occurrs if key is pressed and column bit is enabled
    if((column & 0x10 && controls[1] != 0x0F) || (column & 0x20 && controls[0] != 0x0F)){
        ifRegister |= 0x10;
    }
}

void JOYPAD::keyUp(SDL_Keycode key){
    switch(key){
            // Up
        case SDLK_w:
            controls[0] |= 0x04;
            break;
            // Left
        case SDLK_a:
            controls[0] |= 0x02;
            break;
            // Down
        case SDLK_s:
            controls[0] |= 0x08;
            break;
            // Right
        case SDLK_d:
            controls[0] |= 0x01;
            break;
            // A
        case SDLK_z:
            controls[1] |= 0x01;
            break;
            // B
        case SDLK_x:
            controls[1] |= 0x02;
            break;
            // Start
        case SDLK_LSHIFT:
            controls[1] |= 0x08;
            break;
            // Select
        case SDLK_SPACE:
            controls[1] |= 0x04;
            break;
        default: break;
    }
}

JOYPAD joypad;
