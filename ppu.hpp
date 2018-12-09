#ifndef ppu_hpp
#define ppu_hpp

#include <SDL2/SDL.h>
#include "definitions.hpp"

// 160 * 144 * 4 == width * height * rgba
#define FRAME_BUFFER_LENGTH 92160

class PPU{
    
    int mode = 2;
    int clock = 0;
    
    SDL_Window *window;
    SDL_Renderer *renderer;
    
    // width * height * (r, g, b, a) where each of {r, g, b, a} is a byte value
    BYTE frameBuffer[FRAME_BUFFER_LENGTH];
    
    void initTileSet();
    void initSpriteSet();
    void initVideo();
    
    void renderImage();
    void renderBackground(BYTE scanRow[160]);
    void renderWindow(BYTE scanRow[160]);
    void renderSprites(BYTE scanRow[160]);
    void renderScan();
    
    void setLCDStatus();
    
public:
    
    void reset();
    void step();
    void quit();    
    void addToClock(int clockCycles);
};

extern PPU ppu;

#endif /* ppu_hpp */
