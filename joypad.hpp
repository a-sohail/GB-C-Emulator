#ifndef joypad_hpp
#define joypad_hpp

#include <SDL2/SDL.h>
#include "definitions.hpp"

class JOYPAD{
    
    BYTE controls[2] = {0x0F, 0x0F};
    BYTE column = 0x0;
    
public:
    
    BYTE readByte();
    void writeByte(BYTE val);
    void reset();
    void keyDown(SDL_Keycode key);
    void keyUp(SDL_Keycode key);
};

extern JOYPAD joypad;

#endif /* joypad_hpp */
