#include "ppu.hpp"
#include "mmu.hpp"

void PPU::initTileSet(){
    for(int tile = 0; tile < MAX_TILES; tile++){
        for(int y = 0; y < 8; y++){
            for(int x = 0; x < 8; x++){
                mmu.tileSet[tile][y][x] = 0;
            }
        }
    }
}

void PPU::initSpriteSet(){
    for(int i = 0, addr = 0xFE00; i < 40; i++, addr+=4){
        mmu.writeByte(addr, 0);
        mmu.writeByte(addr + 1, 0);
        mmu.writeByte(addr + 2, 0);
        mmu.writeByte(addr + 3, 0);
    }
}

void PPU::initVideo(){
    // width, height, flags, window, renderer
    //SDL_CreateWindowAndRenderer(160, 144, 0, &window, &renderer);
    window = SDL_CreateWindow("Gameboy", SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED, 160, 144, SDL_WINDOW_SHOWN);
    // Create renderer for window
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

void PPU::renderImage(){
    for(int y = 0; y < 144; y++){
        for(int x = 0; x < 160; x++){
            int pixel = (y * 160 + x) * 4;
            SDL_SetRenderDrawColor(renderer, frameBuffer[pixel], frameBuffer[pixel + 1], frameBuffer[pixel + 2], frameBuffer[pixel + 3]);
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }
    SDL_RenderPresent(renderer);
}

void PPU::renderBackground(BYTE scanRow[160]){
    
    // Determine which map to use
    WORD mapOffset = mmu.bgMap ? 0x9C00 : 0x9800;
    
    // Determine where to draw on screen (framebuffer)
    int screenOffset = mmu.line * 160 * 4;
    
    for(int column = 0; column < 160; column++){
        
        // Work out the index of the pixel in the framebuffer
        WORD lineOffset = (mmu.scrollX + column  ) % 256;
        WORD rowOffset  = (mmu.scrollY + mmu.line) % 256;
        
        // Work out the tile for this pixel
        WORD tileX = lineOffset / 8;
        WORD tileY = rowOffset / 8;
        
        // Work out the index of the tile in the array of all tiles
        WORD tileIndex = tileY * 32 + tileX;
        WORD tileIDAddress = mapOffset + tileIndex;
        
        // Narrow down exactly which row of pixels and offset to start from
        int y = rowOffset % 8;
        int x = lineOffset % 8;
        
        int tile = mmu.readByte(tileIDAddress);
        
        if(!mmu.bgTile && tile < 128){
            tile += 256;
        }
        
        // Map to palette
        BYTE colour[4];
        for(int i = 0; i < 4; i++){
            colour[i] = mmu.palette[mmu.tileSet[tile][y][x]][i];
        }
        scanRow[column] = mmu.tileSet[tile][y][x];
        frameBuffer[screenOffset] = colour[0];
        frameBuffer[screenOffset + 1] = colour[1];
        frameBuffer[screenOffset + 2] = colour[2];
        frameBuffer[screenOffset + 3] = colour[3];
        screenOffset += 4;
    }
}

void PPU::renderWindow(BYTE scanRow[160]){
    
    if (mmu.line < mmu.windowY){
        return;
    }
    
    // Determine which map to use
    WORD mapOffset = mmu.windowTile ? 0x9C00 : 0x9800;
    
    // Determine where to draw on screen (framebuffer)
    int screenOffset = mmu.line * 160 * 4;
    
    for(int column = 0; column < 160; column++){
        
        // Work out the index of the pixel in the framebuffer
        WORD lineOffset = mmu.windowX + column - 7;
        WORD rowOffset = mmu.line - mmu.windowY;
        
        // Work out the tile for this pixel
        WORD tileX = lineOffset / 8;
        WORD tileY = rowOffset / 8;
        
        // Work out the index of the tile in the array of all tiles
        WORD tileIndex = tileY * 32 + tileX;
        WORD tileIDAddress = mapOffset + tileIndex;
        
        // Narrow down exactly which row of pixels and offset to start from
        int x = lineOffset % 8;
        int y = rowOffset % 8;
        
        int tile = mmu.readByte(tileIDAddress);
        
        if(!mmu.bgTile && tile < 128){
            tile += 256;
        }
        
        // Map to palette
        BYTE colour[4];
        
        for(int i = 0; i < 4; i++){
            colour[i] = mmu.palette[mmu.tileSet[tile][y][x]][i];
        }
        scanRow[column] = mmu.tileSet[tile][y][x];
        frameBuffer[screenOffset] = colour[0];
        frameBuffer[screenOffset + 1] = colour[1];
        frameBuffer[screenOffset + 2] = colour[2];
        frameBuffer[screenOffset + 3] = colour[3];
        screenOffset += 4;
    }
}

void PPU::renderSprites(BYTE scanRow[160]){
    for(int i = 0; i < 40; i++){
        
        SPRITE& sprite = mmu.spriteSet[i];
        
        int height = mmu.spriteDoubled ? 16 : 8;
        if(sprite.posY <= mmu.line && (sprite.posY + height) > mmu.line){
            int screenOffset = (mmu.line * 160 + sprite.posX) * 4;
            
            BYTE tileRow[8];
            for(int j = 0; j < 8; j++){
                tileRow[j] = mmu.tileSet[sprite.tileNumber]
                [sprite.flippedY ?
                 ((height - 1) - (mmu.line - sprite.posY)) :
                 (mmu.line - sprite.posY)
                 ]
                [j];
            }
            
            BYTE palette[4][4];
            for(int j = 0; j < 4; j++){
                for(int k = 0; k < 4; k++){
                    palette[j][k] = sprite.zeroPalette ? mmu.obj0Palette[j][k] : mmu.obj1Palette[j][k];
                }
            }
            
            BYTE colour[4];
            for(int x = 0; x < 8; x++){
                if((sprite.posX + x) >= 0 && (sprite.posX + x) < 160 && (tileRow[sprite.flippedX ? (7 - x) : x]) && (sprite.prioritized || !scanRow[sprite.posX + x])){
                    
                    for(int j = 0; j < 4; j++){
                        colour[j] = palette[tileRow[sprite.flippedX ? (7 - x) : x]][j];
                    }
                    
                    frameBuffer[screenOffset] = colour[0];
                    frameBuffer[screenOffset + 1] = colour[1];
                    frameBuffer[screenOffset + 2] = colour[2];
                    frameBuffer[screenOffset + 3] = colour[3];
                }
                screenOffset += 4;
            }
        }
    }
}

void PPU::renderScan(){
    
    if(!mmu.switchLCD){
        return;
    }
    
    BYTE scanRow[160];
    if(mmu.switchBG){
        renderBackground(scanRow);
    }
    
    if(mmu.switchWindow){
        renderWindow(scanRow);
    }
    
    if(mmu.switchOBJ){
        renderSprites(scanRow);
    }
    
}

void PPU::setLCDStatus(){
    
    BYTE currentMode = mmu.lcdStatRegister & 0x3;
    
    BYTE lcdMode = 0;
    bool shouldInterrupt = false;
    
    // V-Blank
    if(mmu.line >= 144){
        lcdMode = 1;
        mmu.lcdStatRegister &= 0xFC;
        mmu.lcdStatRegister |= 0x01;
        shouldInterrupt = mmu.lcdStatRegister & 0x10;
    }
    else{
        
        int mode2Bounds = (456 - 80);
        int mode3Bounds = (mode2Bounds - 172) * 4;
        mode2Bounds *= 4;
        
        if(clock >= mode2Bounds){
            lcdMode = 2;
            mmu.lcdStatRegister &= 0xFC;
            mmu.lcdStatRegister |= 0x01;
            shouldInterrupt = mmu.lcdStatRegister & 0x20;
        }
        else if(clock >= mode3Bounds){
            lcdMode = 3;
            mmu.lcdStatRegister |= 0x03;
        }
        else{
            lcdMode = 0;
            mmu.lcdStatRegister &= 0xFC;
            shouldInterrupt = mmu.lcdStatRegister & 0x08;
        }
        
    }
    
    if(shouldInterrupt && (lcdMode != currentMode)){
        ifRegister |= 0x2;
    }
    
    // LY = LYC
    if(mmu.line == mmu.readByte(0xFF45)){
        mmu.lcdStatRegister |= 0x4;
        if(mmu.lcdStatRegister & 0x40){
            ifRegister |= 0x2;
        }
    }
    else{
        mmu.lcdStatRegister &= 0xFB;
    }
    
}

void PPU::reset(){
    initTileSet();
    initSpriteSet();
    initVideo();
}

void PPU::step(){
    
    setLCDStatus();
    
    // Screen cycles through (OAM -> VRAM -> HBLANK) * 144 -> VBLANK
    switch (mode){
            // OAM
        case 2:
            
            if (clock >= 320){
                clock %= 320;
                mode = 3;
            }
            break;
            
            // VRAM
        case 3:
            if(clock >= 688){
                clock %= 688;
                mode = 0;
            }
            break;
            
            // HBLANK
        case 0:
            
            if(clock >= 816){
                
                renderScan();
                
                clock %= 816;
                mmu.line++;
                
                //std::cout << "Line: " << std::dec << mmu.line << std::endl;
                
                if(mmu.line == 144){
                    mode = 1;
                    ifRegister |= 0x1;
                }
                else{
                    mode = 2;
                }
            }
            break;
            
            // VBLANK
        case 1:
            if(clock >= 1824){
                clock %= 1824;
                mmu.line++;
                
                if(mmu.line == 154){
                    renderImage();
                    mode = 2;
                    mmu.line = 0;
                }
            }
            break;
    }
}

void PPU::quit(){
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void PPU::addToClock(int clockCycles){
    clock += clockCycles;
}

PPU ppu;
