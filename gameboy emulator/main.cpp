#include <assert.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <cstdio>
#include <string>
#include <SDL2/SDL.h>
#include <algorithm>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Clock Speed
#define CLOCKSPEED 4194304

// register.h
#define FLAG_Z 0x80
#define FLAG_N 0x40
#define FLAG_HC 0x20
#define FLAG_C 0x10

// 2^16 spots
#define MAX_MEMORY 0x200000

// 160 * 144 * 4 == width * height * rgba
#define FRAME_BUFFER_LENGTH 92160

// 384 max tiles allowed in VRAM
#define MAX_TILES 384

typedef unsigned char BYTE;
typedef char SIGNED_BYTE;
typedef unsigned short WORD;
typedef short SIGNED_WORD;

union Register{
    WORD reg;
    struct{
        BYTE lo;
        BYTE hi;
    };
};

Register AF;
Register BC;
Register DE;
Register HL;
Register SP;
WORD PC;
WORD ifRegister;
WORD ieRegister;

// Interrupts
bool IME = false;
bool halt = false;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Class Definitions
class CPU;
class PPU;
class MMU;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// bitOperations
void flagBit(BYTE& reg, const BYTE& b){
    reg |= b;
    if(reg == AF.lo){
        AF.lo &= 0xF0;
    }
}

void unflagBit(BYTE& reg, const BYTE& b){
    reg &= ~b;
    if(reg == AF.lo){
        AF.lo &= 0xF0;
    }
}

bool isFlagged(BYTE& reg, const BYTE& b){
    return (reg & b) != 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class JOYPAD{
    BYTE controls[2] = {0x0F, 0x0F};
    BYTE column = 0x0;
    
public:
    BYTE readByte(){
        switch (column) {
            case 0x10:
                return controls[1];
            case 0x20:
                return controls[0];
            default:
                return 0x0;
        }
    }
    
    void writeByte(BYTE val){
        // Only care about bits 4 and 5
        column = val & 0x30;
    }
    
    void reset(){
        controls[0] = 0x0F;
        controls[1] = 0x0F;
        column = 0x0;
    }
    
    void keyDown(SDL_Keycode key){
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
    
    void keyUp(SDL_Keycode key){
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
};

JOYPAD joypad;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SPRITE{
    
    SIGNED_WORD posY;
    SIGNED_WORD posX;
    BYTE tileNumber;
    bool prioritized;
    bool flippedY;
    bool flippedX;
    bool zeroPalette;
    
public:
    
    void setPosY(SIGNED_WORD posY){
        this->posY = posY;
    }
    SIGNED_WORD getPosY(){
        return posY;
    }
    
    void setPosX(SIGNED_WORD posX){
        this->posX = posX;
    }
    SIGNED_WORD getPosX(){
        return posX;
    }
    
    void setTileNumber(BYTE tileNumber){
        this->tileNumber = tileNumber;
    }
    BYTE getTileNumber(){
        return tileNumber;
    }
    
    void setPriority(bool priority){
        this->prioritized = priority;
    }
    bool isPrioritized(){
        return prioritized;
    }
    
    void setFlippedY(bool flippedY){
        this->flippedY = flippedY;
    }
    bool isFlippedY(){
        return flippedY;
    }
    
    void setFlippedX(bool flippedX){
        this->flippedX = flippedX;
    }
    bool isFlippedX(){
        return flippedX;
    }
    
    void setZeroPalette(bool zeroPalette){
        this->zeroPalette = zeroPalette;
    }
    bool isZeroPalette(){
        return zeroPalette;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class Timer{
    
public:
    
    BYTE divider = 0;
    BYTE counter = 0;
    BYTE modulo = 0;
    BYTE control = 0;
    
    int controlClock = 1024;
    int dividerClock = 0;
    
    bool isClockEnabled = true;
    
    void addToClock(int clockCycles){
        
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
    
    void setControlRate(){
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
    
};
Timer timer;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class MMU{
    
    // PPU Registers and Tile Set
public:
    // Internal tile set of 8x8 pixels
    BYTE tileSet[MAX_TILES][8][8];
    
    // Internal Sprite Map
    SPRITE spriteSet[40];
    
    bool bgMap;
    bool bgTile;
    bool switchBG;
    bool switchOBJ;
    bool switchLCD;
    
    BYTE scrollX;
    BYTE scrollY;
    
    int line = 0;
    
    // Given a pixel labelled 0-3, return an array with RGBA values
    BYTE palette[4][4];
    BYTE obj0Palette[4][4];
    BYTE obj1Palette[4][4];
    
    BYTE lcdStatRegister = 0;
    
    // Memory Banking
    bool mbc1 = false;
    BYTE romBankNumber = 0x01;
    BYTE ramBankNumber = 0x0;
    bool ramEnabled = false;
    bool romMode = true;
    
private:
    BYTE memory[MAX_MEMORY];
    BYTE ramMemory[0x8000];
    
    bool inBIOS = true;
    
    BYTE bootROM[256] = {
        0x31, 0xFE, 0xFF, 0xAF, 0x21, 0xFF, 0x9F, 0x32, 0xCB, 0x7C, 0x20, 0xFB, 0x21, 0x26, 0xFF, 0x0E,
        0x11, 0x3E, 0x80, 0x32, 0xE2, 0x0C, 0x3E, 0xF3, 0xE2, 0x32, 0x3E, 0x77, 0x77, 0x3E, 0xFC, 0xE0,
        0x47, 0x11, 0x04, 0x01, 0x21, 0x10, 0x80, 0x1A, 0xCD, 0x95, 0x00, 0xCD, 0x96, 0x00, 0x13, 0x7B,
        0xFE, 0x34, 0x20, 0xF3, 0x11, 0xD8, 0x00, 0x06, 0x08, 0x1A, 0x13, 0x22, 0x23, 0x05, 0x20, 0xF9,
        0x3E, 0x19, 0xEA, 0x10, 0x99, 0x21, 0x2F, 0x99, 0x0E, 0x0C, 0x3D, 0x28, 0x08, 0x32, 0x0D, 0x20,
        0xF9, 0x2E, 0x0F, 0x18, 0xF3, 0x67, 0x3E, 0x64, 0x57, 0xE0, 0x42, 0x3E, 0x91, 0xE0, 0x40, 0x04,
        0x1E, 0x02, 0x0E, 0x0C, 0xF0, 0x44, 0xFE, 0x90, 0x20, 0xFA, 0x0D, 0x20, 0xF7, 0x1D, 0x20, 0xF2,
        0x0E, 0x13, 0x24, 0x7C, 0x1E, 0x83, 0xFE, 0x62, 0x28, 0x06, 0x1E, 0xC1, 0xFE, 0x64, 0x20, 0x06,
        0x7B, 0xE2, 0x0C, 0x3E, 0x87, 0xE2, 0xF0, 0x42, 0x90, 0xE0, 0x42, 0x15, 0x20, 0xD2, 0x05, 0x20,
        0x4F, 0x16, 0x20, 0x18, 0xCB, 0x4F, 0x06, 0x04, 0xC5, 0xCB, 0x11, 0x17, 0xC1, 0xCB, 0x11, 0x17,
        0x05, 0x20, 0xF5, 0x22, 0x23, 0x22, 0x23, 0xC9, 0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
        0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
        0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
        0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E, 0x3C, 0x42, 0xB9, 0xA5, 0xB9, 0xA5, 0x42, 0x3C,
        0x21, 0x04, 0x01, 0x11, 0xA8, 0x00, 0x1A, 0x13, 0xBE, 0x00, 0x00, 0x23, 0x7D, 0xFE, 0x34, 0x20,
        0xF5, 0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 0xFB, 0x86, 0x00, 0x00, 0x3E, 0x01, 0xE0, 0x50
    };
    
    void updateTileSet(WORD addr){
        // Every pixel is 2 rows and we'll start indexing from 0
        WORD baseAddr = addr & 0x1FFE;
        
        // Divide by 16 to get tile
        int tile = (baseAddr >> 4) & 0x1FF;
        // Divide by 2 and mod by 8 to get which row
        int y = (baseAddr >> 1) % 8;
        
        for(int x = 0; x < 8; x++){
            BYTE bitIndex = 1 << (7 - x);
            BYTE val = ((readByte(addr & 0xFFFE) & bitIndex) ? 1 : 0) + ((readByte((addr & 0xFFFE) + 1) & bitIndex) ? 2 : 0);
            
            tileSet[tile][y][x] = val;
        }
    }
    
    void updateSpriteSet(WORD addr, BYTE val){
        int spriteNumber = (addr - 0xFE00) >> 2;
        
        SPRITE& sprite = spriteSet[spriteNumber];

        switch((addr - 0xFE00) & 0x03){
            case 0:
                 sprite.setPosY(val - 16);
                break;
            case 1:
                sprite.setPosX(val - 8);
                break;
            case 2:
                sprite.setTileNumber(val);
                break;
            case 3:
                sprite.setPriority((val & 0x80) ? 0 : 1);
                sprite.setFlippedY((val & 0x40) ? 1 : 0);
                sprite.setFlippedX((val & 0x20) ? 1 : 0);
                sprite.setZeroPalette((val & 0x10) ? 0 : 1);
                break;
        }
    }
    
    void setPalette(BYTE palette[4][4], BYTE val){
        for(int i = 0; i < 4; i++){
            switch((val >> (i * 2)) & 3){
                case 0: palette[i][0] = 255; palette[i][1] = 255; palette[i][2] = 255; palette[i][3] = 255; break;
                case 1: palette[i][0] = 192; palette[i][1] = 192; palette[i][2] = 192; palette[i][3] = 255; break;
                case 2: palette[i][0] = 96; palette[i][1] = 96; palette[i][2] = 96; palette[i][3] = 255; break;
                case 3: palette[i][0] = 0; palette[i][1] = 0; palette[i][2] = 0; palette[i][3] = 255; break;
            }
        }
    }
    
    void dmaTransfer(const BYTE& val) {
        WORD startAddr = val * 0x100;
        
        for (BYTE i = 0x0; i <= 0x9F; i++) {
            WORD fromAddr = startAddr + i;
            WORD toAddr = 0xFE00 + i;
            
            BYTE valAtAddr = readByte(fromAddr);
            writeByte(toAddr, valAtAddr);
        }
    }
    
public:
    
    void reset(){
        memset(&memory, 0, MAX_MEMORY);
        memset(&ramMemory, 0, sizeof(ramMemory));
    }
    
    void updateBanking(){
        switch(memory[0x147]){
            case 1  : mbc1 = true; break;
            case 2  : mbc1 = true; break;
            default : mbc1 = false; break;
        }
    }
    
    void handleBanking(WORD address, BYTE val){
        if(!mbc1){
            return;
        }
        
        if(address < 0x2000){
            ramEnabled = (val & 0x0F) == 0x0A;
        }
        else if(address < 0x4000){
            val = (romBankNumber & 0xE0) | (val & 0x1F);
            switch(val){
                case 0x00:
                case 0x20:
                case 0x40:
                case 0x60:
                    val++;
                default: break;
            }
            romBankNumber = val;
        }
        else if(address < 0x6000){
            if(romMode){
                romBankNumber = (val & 0xE0) | (romBankNumber & 0x1F);
                if(!romBankNumber){
                    romBankNumber++;
                }
            }
            else{
                ramBankNumber = val & 0x3;
            }
        }
        else if(address < 0x8000){
            romMode = (val & 0x1) == 0x00;
            if(romMode){
                ramBankNumber = 0;
            }
        }
        
    }
    
    BYTE readByte(WORD address){
        
        if(inBIOS && address < 0x100){
            return bootROM[address];
        }
        else if (address >= 0x4000 && address <= 0x7FFF){
            return memory[(address - 0x4000) + (romBankNumber * 0x4000)];
        }
        else if(address >= 0xA000 && address <= 0xBFFF){
            return ramMemory[(address - 0xA000) + (ramBankNumber * 0x2000)];
        }
        else if (address == 0xFF00){
            return joypad.readByte();
        }
        else if(address == 0xFF04){
            return timer.divider;
        }
        else if(address == 0xFF05){
            return timer.counter;
        }
        else if(address == 0xFF06){
            return timer.modulo;
        }
        else if(address == 0xFF07){
            return timer.control & 0x3;
        }
        else if(address == 0xFF40){
            return (switchBG  ? 0x01 : 0x00) |
                   (switchOBJ ? 0x02 : 0x00) |
                   (bgMap     ? 0x08 : 0x00) |
                   (bgTile    ? 0x10 : 0x00) |
                   (switchLCD ? 0x80 : 0x00);
        }
        else if(address == 0xFF41){
            return lcdStatRegister;
        }
        else if(address == 0xFF42){
            return scrollY;
        }
        else if(address == 0xFF43){
            return scrollX;
        }
        else if(address == 0xFF44){
            return line;
        }
        else if(address == 0xFF0F){
            return ifRegister;
        }
        else if(address == 0xFFFF){
            return ieRegister;
        }
        
        return memory[address];
    }
    
    void writeByte(WORD address, BYTE val){
        
        if(address >= 0xFEA0 && address <= 0xFEFF){
            return;
        }
        else if(address < 0x8000){
            handleBanking(address, val);
        }
        else if(address >= 0xA000 && address < 0xC000){
            if(ramEnabled){
                ramMemory[(address - 0xA000) + (ramBankNumber * 0x2000)] = val;
            }
            return;
        }
        // Echo
        else if(address >= 0xE000 && address <= 0xFDFF){
            writeByte(address - 0x2000, val);
        }
        // VRAM Tile Set
        else if(address >= 0x8000 && address <= 0x97FF){
            memory[address] = val;
            updateTileSet(address);
            return;
        }
        else if(address >= 0xFE00 && address <= 0xFE9F){
            memory[address] = val;
            updateSpriteSet(address, val);
            return;
        }
        else if(address == 0xFF00){
            joypad.writeByte(val);
            return;
        }
        else if(address == 0xFF04){
            timer.divider = 0;
            return;
        }
        else if(address == 0xFF05){
            timer.counter = val;
            return;
        }
        else if(address == 0xFF06){
            timer.modulo = val;
            return;
        }
        else if(address == 0xFF07){
            timer.control = val;
            timer.setControlRate();
            return;
        }
        else if (address == 0xFF0F){
            ifRegister = 0x1F & val;
            return;
        }
        else if(address == 0xFF40){
            switchBG  = (val & 0x01) ? 1 : 0;
            switchOBJ = (val & 0x02) ? 1 : 0;
            bgMap     = (val & 0x08) ? 1 : 0;
            bgTile    = (val & 0x10) ? 1 : 0;
            switchLCD = (val & 0x80) ? 1 : 0;
            return;
        }
        else if(address == 0xFF41){
            lcdStatRegister = (val & 0x78) | (memory[address] & 0x07);
            return;
        }
        else if(address == 0xFF42){
            scrollY = val;
            return;
        }
        else if(address == 0xFF43){
            scrollX = val;
            return;
        }
        else if(address == 0xFF46){
            dmaTransfer(val);
        }
        else if(address == 0xFF47){
            setPalette(palette, val);
            return;
        }
        else if(address == 0xFF48){
            setPalette(obj0Palette, val);
            return;
        }
        else if(address == 0xFF49){
            setPalette(obj1Palette, val);
            return;
        }
        else if(address == 0xFF50){
            inBIOS = false;
            return;
        }
        else if(address == 0xFFFF){
            ieRegister = val;
            return;
        }
        
        memory[address] = val;
    }
    
    WORD readWord(WORD address){
        BYTE lowerByte = readByte(address);
        BYTE higherByte = readByte(address + 1);
        return lowerByte | (higherByte << 8);
    }
    
    void writeWord(WORD address, WORD val){
        writeByte(address, val);
        writeByte(address + 1, val >> 8);
    }
    
    // Read Rom
    void readROM(const std::string& rom){
        FILE * file = fopen(rom.c_str(), "rb");
        if (file == NULL) return;
        fread(memory, sizeof(BYTE), MAX_MEMORY, file);
        fclose(file);
    }
};

MMU mmu;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CPU{
    
    int clock = 0;
    
    // 8-bit loads
    void CPU_LOAD(BYTE& b1, const BYTE& b2){
        b1 = b2;
    }
    
    void CPU_LOAD_WRITE(const WORD& w, const BYTE& b){
        mmu.writeByte(w, b);
    }
    
    // 16-bit loads
    void CPU_LOAD_16BIT(WORD& reg, const WORD& val, const SIGNED_BYTE& immValue, bool isStackPointer){
        reg = val;
        if(isStackPointer){
            int result = val + immValue;
            AF.lo = 0;
            if(((val ^ immValue ^ (result & 0xFFFF)) & 0x10) == 0x10){
                flagBit(AF.lo, FLAG_HC);
            }
            if(((val ^ immValue ^ (result & 0xFFFF)) & 0x100) == 0x100){
                flagBit(AF.lo, FLAG_C);
            }
            reg = result;
        }
    }
    
    void CPU_LOAD_WRITE_16BIT(const WORD& w, const WORD& b){
        mmu.writeWord(w, b);
    }
    
    void CPU_PUSH(const WORD& reg){
        SP.reg -= 2;
        mmu.writeWord(SP.reg, reg);
    }
    
    void CPU_POP(WORD& reg){
        reg = mmu.readWord(SP.reg);
        SP.reg += 2;
    }
    
    // 8-bit Arithmetic/Logical Commands
    void CPU_ADD(const BYTE& b, bool carry){
        BYTE prev = AF.hi;
        WORD adding = 0;
        BYTE carryVal = 0x0;
        adding += b;
        if(carry && isFlagged(AF.lo, FLAG_C)){
            carryVal = 0x1;
        }
        AF.hi += adding + carryVal;
        
        uint result = prev + adding + carryVal;
        // Flags
        AF.lo = 0;
        if(AF.hi == 0){
            flagBit(AF.lo, FLAG_Z);
        }
        if((prev & 0xF) + (adding & 0xF) + carryVal > 0xF){
            flagBit(AF.lo, FLAG_HC);
        }
        if((result & 0x100) != 0){
            flagBit(AF.lo, FLAG_C);
        }
    }
    
    void CPU_SUB(const BYTE& b, bool carry){
        BYTE prev = AF.hi;
        BYTE carryVal = 0x0;
        
        if(carry && isFlagged(AF.lo, FLAG_C)){
            carryVal = 0x1;
        }
        
        int subtracting = prev - b - carryVal;
        BYTE result = static_cast<BYTE>(subtracting);
        
        // Flags
        AF.lo = 0;
        flagBit(AF.lo, FLAG_N);
        if(result == 0){
            flagBit(AF.lo, FLAG_Z);
        }
        if(((prev & 0xf) - (b & 0xf) - carryVal) < 0){
            flagBit(AF.lo, FLAG_HC);
        }
        if(subtracting < 0){
            flagBit(AF.lo, FLAG_C);
        }
        
        AF.hi = result;
    }
    
    void CPU_AND(const BYTE& b){
        AF.hi &= b;
        AF.lo = 0;
        if(AF.hi == 0){
            flagBit(AF.lo, FLAG_Z);
        }
        flagBit(AF.lo, FLAG_HC);
    }
    
    void CPU_XOR(const BYTE& b){
        AF.hi ^= b;
        AF.lo = 0;
        if(AF.hi == 0){
            flagBit(AF.lo, FLAG_Z);
        }
    }
    
    void CPU_OR(const BYTE& b){
        AF.hi |= b;
        AF.lo = 0;
        if(AF.hi == 0){
            flagBit(AF.lo, FLAG_Z);
        }
    }
    
    void CPU_CP(const BYTE& b){
        BYTE regA = AF.hi - b;
        // Flags
        AF.lo = 0;
        flagBit(AF.lo, FLAG_N);
        if(regA == 0){
            flagBit(AF.lo, FLAG_Z);
        }
        if((AF.hi & 0xF) - (b & 0xF) < 0){
            flagBit(AF.lo, FLAG_HC);
        }
        if(AF.hi < b){
            flagBit(AF.lo, FLAG_C);
        }
    }
    
    void CPU_INC(BYTE& b){
        b++;
        if(b == 0){
            flagBit(AF.lo, FLAG_Z);
        }
        else{
            unflagBit(AF.lo, FLAG_Z);
        }
        unflagBit(AF.lo, FLAG_N);
        if((b & 0x0F) == 0x00){
            flagBit(AF.lo, FLAG_HC);
        }
        else{
            unflagBit(AF.lo, FLAG_HC);
        }
    }
    
    void CPU_INC_WRITE(){
        BYTE before = mmu.readByte(HL.reg);
        before++;
        mmu.writeByte(HL.reg, before);
        if(before == 0){
            flagBit(AF.lo, FLAG_Z);
        }
        else{
            unflagBit(AF.lo, FLAG_Z);
        }
        unflagBit(AF.lo, FLAG_N);
        if((before & 0x0F) == 0x00){
            flagBit(AF.lo, FLAG_HC);
        }
        else{
            unflagBit(AF.lo, FLAG_HC);
        }
    }
    
    void CPU_DEC(BYTE& b){
        b--;
        if(b == 0){
            flagBit(AF.lo, FLAG_Z);
        }
        else{
            unflagBit(AF.lo, FLAG_Z);
        }
        flagBit(AF.lo, FLAG_N);
        if((b & 0x0F) == 0x0F){
            flagBit(AF.lo, FLAG_HC);
        }
        else{
            unflagBit(AF.lo, FLAG_HC);
        }
    }
    
    void CPU_DEC_WRITE(){
        BYTE before = mmu.readByte(HL.reg);
        mmu.writeByte(HL.reg, before - 1);
        if(before - 1 == 0){
            flagBit(AF.lo, FLAG_Z);
        }
        else{
            unflagBit(AF.lo, FLAG_Z);
        }
        flagBit(AF.lo, FLAG_N);
        if((before - 1 & 0x0F) == 0x0F){
            flagBit(AF.lo, FLAG_HC);
        }
        else{
            unflagBit(AF.lo, FLAG_HC);
        }
    }
    
    void CPU_DAA(){
        if(!isFlagged(AF.lo, FLAG_N)){
            if(isFlagged(AF.lo, FLAG_C) || AF.hi > 0x99){
                AF.hi += 0x60;
                flagBit(AF.lo, FLAG_C);
            }
            if(isFlagged(AF.lo, FLAG_HC) || (AF.hi & 0x0F) > 0x09){
                AF.hi += 0x6;
            }
        }
        else{
            if(isFlagged(AF.lo, FLAG_C)){
                AF.hi -= 0x60;
            }
            if(isFlagged(AF.lo, FLAG_HC)){
                AF.hi -= 0x6;
            }
        }
        
        unflagBit(AF.lo, FLAG_Z);
        unflagBit(AF.lo, FLAG_HC);
        
        if(AF.hi == 0){
            flagBit(AF.lo, FLAG_Z);
        }
    }
    
    void CPU_CPL(){
        AF.hi = ~AF.hi;
        flagBit(AF.lo, FLAG_N);
        flagBit(AF.lo, FLAG_HC);
    }
    
    // 16-bit Arithmetic/Logical Commands
    void CPU_ADD_16BIT(WORD& reg, const WORD& val){
        uint result = reg + val;
        WORD prev = reg;
        reg += val;
        if((prev & 0xFFF) + (val & 0xFFF) > 0xFFF){
            flagBit(AF.lo, FLAG_HC);
        }
        else{
            unflagBit(AF.lo, FLAG_HC);
        }
        if((result & 0x10000) != 0){
            flagBit(AF.lo, FLAG_C);
        }
        else{
            unflagBit(AF.lo, FLAG_C);
        }
        unflagBit(AF.lo, FLAG_N);
    }
    
    void CPU_ADD_16BIT_SIGNED(WORD& reg, const SIGNED_BYTE& val){
        WORD prev = reg;
        int result = static_cast<int>(prev + val);
        
        if(((prev ^ val ^ (result & 0xFFFF)) & 0x10) == 0x10){
            flagBit(AF.lo, FLAG_HC);
        }
        else{
            unflagBit(AF.lo, FLAG_HC);
        }
        if(((prev ^ val ^ (result & 0xFFFF)) & 0x100) == 0x100){
            flagBit(AF.lo, FLAG_C);
        }
        else{
            unflagBit(AF.lo, FLAG_C);
        }
        unflagBit(AF.lo, FLAG_N);
        unflagBit(AF.lo, FLAG_Z);
        
        reg = static_cast<WORD>(result);
    }
    
    void CPU_INC_16BIT(WORD& reg){
        reg++;
    }
    
    void CPU_DEC_16BIT(WORD& reg){
        reg--;
    }
    
    void CPU_RLC(BYTE& reg){
        bool msb = isFlagged(reg, 0x80);
        reg <<= 1;
        AF.lo = 0;
        if(msb){
            flagBit(reg, 0x01);
            flagBit(AF.lo, FLAG_C);
        }
        if(reg == 0){
            flagBit(AF.lo, FLAG_Z);
        }
    }
    
    void CPU_RLCA(BYTE& reg){
        bool msb = isFlagged(reg, 0x80);
        reg <<= 1;
        AF.lo = 0;
        if(msb){
            flagBit(reg, 0x01);
            flagBit(AF.lo, FLAG_C);
        }
    }
    
    void CPU_RL(BYTE& reg){
        bool msb = isFlagged(reg, 0x80);
        bool carry = isFlagged(AF.lo, FLAG_C);
        reg <<= 1;
        AF.lo = 0;
        if(msb){
            flagBit(AF.lo, FLAG_C);
        }
        if(carry){
            flagBit(reg, 0x01);
        }
        if(reg == 0){
            flagBit(AF.lo, FLAG_Z);
        }
    }
    
    void CPU_RLA(BYTE& reg){
        bool msb = isFlagged(reg, 0x80);
        bool carry = isFlagged(AF.lo, FLAG_C);
        reg <<= 1;
        AF.lo = 0;
        if(msb){
            flagBit(AF.lo, FLAG_C);
        }
        if(carry){
            flagBit(reg, 0x01);
        }
    }
    
    void CPU_RRC(BYTE& reg){
        bool lsb = isFlagged(reg, 0x01);
        reg >>= 1;
        AF.lo = 0;
        if(lsb){
            flagBit(reg, 0x80);
            flagBit(AF.lo, FLAG_C);
        }
        if(reg == 0){
            flagBit(AF.lo, FLAG_Z);
        }
    }
    
    void CPU_RR(BYTE& reg){
        bool lsb = isFlagged(reg, 0x01);
        bool carry = isFlagged(AF.lo, FLAG_C);
        reg >>= 1;
        AF.lo = 0;
        if(lsb){
            flagBit(AF.lo, FLAG_C);
        }
        if(carry){
            flagBit(reg, 0x80);
        }
        if(reg == 0){
            flagBit(AF.lo, FLAG_Z);
        }
    }
    
    void CPU_RRCA(BYTE& reg){
        bool lsb = isFlagged(reg, 0x01);
        reg >>= 1;
        AF.lo = 0;
        if(lsb){
            flagBit(reg, 0x80);
            flagBit(AF.lo, FLAG_C);
        }
    }
    
    void CPU_RRA(BYTE& reg){
        bool lsb = isFlagged(reg, 0x01);
        bool carry = isFlagged(AF.lo, FLAG_C);
        reg >>= 1;
        AF.lo = 0;
        if(lsb){
            flagBit(AF.lo, FLAG_C);
        }
        if(carry){
            flagBit(reg, 0x80);
        }
    }
    
    void CPU_RLC_WRITE(){
        BYTE reg = mmu.readByte(HL.reg);
        bool msb = isFlagged(reg, 0x80);
        reg <<= 1;
        AF.lo = 0;
        if(msb){
            flagBit(AF.lo, FLAG_C);
            flagBit(reg, 0x01);
        }
        mmu.writeByte(HL.reg, reg);
        if(reg == 0){
            flagBit(AF.lo, FLAG_Z);
        }
    }
    
    void CPU_RL_WRITE(){
        BYTE reg = mmu.readByte(HL.reg);
        bool msb = isFlagged(reg, 0x80);
        bool carry = isFlagged(AF.lo, FLAG_C);
        reg <<= 1;
        AF.lo = 0;
        if(msb){
            flagBit(AF.lo, FLAG_C);
        }
        if(carry){
            flagBit(reg, 0x01);
        }
        mmu.writeByte(HL.reg, reg);
        if(reg == 0){
            flagBit(AF.lo, FLAG_Z);
        }
    }
    
    void CPU_RRC_WRITE(){
        BYTE reg = mmu.readByte(HL.reg);
        bool lsb = isFlagged(reg, 0x01);
        reg >>= 1;
        AF.lo = 0;
        if(lsb){
            flagBit(AF.lo, FLAG_C);
            flagBit(reg, 0x80);
        }
        mmu.writeByte(HL.reg, reg);
        if(reg == 0){
            flagBit(AF.lo, FLAG_Z);
        }
    }
    
    void CPU_RR_WRITE(){
        BYTE reg = mmu.readByte(HL.reg);
        bool lsb = isFlagged(reg, 0x01);
        bool carry = isFlagged(AF.lo, FLAG_C);
        reg >>= 1;
        AF.lo = 0;
        if(lsb){
            flagBit(AF.lo, FLAG_C);
        }
        if(carry){
            flagBit(reg, 0x80);
        }
        mmu.writeByte(HL.reg, reg);
        if(reg == 0){
            flagBit(AF.lo, FLAG_Z);
        }
    }
    
    void CPU_SLA(BYTE& reg){
        bool msb = isFlagged(reg, 0x80);
        reg <<= 1;
        AF.lo = 0;
        if(msb){
            flagBit(AF.lo, FLAG_C);
        }
        if(reg == 0){
            flagBit(AF.lo, FLAG_Z);
        }
    }
    
    void CPU_SLA_WRITE(){
        BYTE reg = mmu.readByte(HL.reg);
        bool msb = isFlagged(reg, 0x80);
        reg <<= 1;
        mmu.writeByte(HL.reg, reg);
        AF.lo = 0;
        if(msb){
            flagBit(AF.lo, FLAG_C);
        }
        if(reg == 0){
            flagBit(AF.lo, FLAG_Z);
        }
    }
    
    void CPU_SWAP(BYTE& reg){
        reg = (reg >> 4) | (reg << 4);
        AF.lo = 0;
        if(reg == 0){
            flagBit(AF.lo, FLAG_Z);
        }
    }
    
    void CPU_SWAP_WRITE(){
        BYTE reg = mmu.readByte(HL.reg);
        reg = (reg >> 4) | (reg << 4);
        mmu.writeByte(HL.reg, reg);
        AF.lo = 0;
        if(reg == 0){
            flagBit(AF.lo, FLAG_Z);
        }
    }
    
    void CPU_SRA(BYTE& reg){
        bool msb = isFlagged(reg, 0x80);
        bool carryBit = isFlagged(reg, 0x1);
        reg >>= 1;
        AF.lo = 0;
        if(msb){
            flagBit(reg, 0x80);
        }
        if(carryBit){
            flagBit(AF.lo, FLAG_C);
        }
        if(reg == 0){
            flagBit(AF.lo, FLAG_Z);
        }
    }
    
    void CPU_SRA_WRITE(){
        BYTE reg = mmu.readByte(HL.reg);
        bool msb = isFlagged(reg, 0x80);
        bool carryBit = isFlagged(reg, 0x1);
        reg >>= 1;
        
        AF.lo = 0;
        if(msb){
            flagBit(reg, 0x80);
        }
        if(carryBit){
            flagBit(AF.lo, FLAG_C);
        }
        mmu.writeByte(HL.reg, reg);
        if(reg == 0){
            flagBit(AF.lo, FLAG_Z);
        }
    }
    
    void CPU_SRL(BYTE& reg){
        bool msb = isFlagged(reg, 0x1);
        reg >>= 1;
        AF.lo = 0;
        if(msb){
            flagBit(AF.lo, FLAG_C);
        }
        if(reg == 0){
            flagBit(AF.lo, FLAG_Z);
        }
    }
    
    void CPU_SRL_WRITE(){
        BYTE reg = mmu.readByte(HL.reg);
        bool msb = isFlagged(reg, 0x1);
        reg >>= 1;
        mmu.writeByte(HL.reg, reg);
        AF.lo = 0;
        if(msb){
            flagBit(AF.lo, FLAG_C);
        }
        if(reg == 0){
            flagBit(AF.lo, FLAG_Z);
        }
    }
    
    // 1-bit Operations
    void CPU_BIT(const BYTE& bit, const BYTE& reg){
        WORD mask = 256;
        if(((mask >> (8 - bit)) & 0xFF & reg) == 0){
            flagBit(AF.lo, FLAG_Z);
        }
        else{
            unflagBit(AF.lo, FLAG_Z);
        }
        unflagBit(AF.lo, FLAG_N);
        flagBit(AF.lo, FLAG_HC);
    }
    
    void CPU_SET(const BYTE& bit, BYTE& reg){
        WORD mask = 256;
        reg = ((mask >> (8 - bit)) & 0xFF) | reg;
    }
    
    void CPU_SET_WRITE(const BYTE& bit){
        BYTE reg = mmu.readByte(HL.reg);
        WORD mask = 256;
        reg = ((mask >> (8 - bit)) & 0xFF) | reg;
        mmu.writeByte(HL.reg, reg);
    }
    
    void CPU_RES(const BYTE& bit, BYTE& reg){
        WORD mask = 256;
        reg = ~((mask >> (8 - bit)) & 0xFF) & reg;
    }
    
    void CPU_RES_WRITE(const BYTE& bit){
        BYTE reg = mmu.readByte(HL.reg);
        WORD mask = 256;
        reg = ~((mask >> (8 - bit)) & 0xFF) & reg;
        mmu.writeByte(HL.reg, reg);
    }
    
    // CPU Control
    void CPU_CCF(){
        unflagBit(AF.lo, FLAG_N);
        unflagBit(AF.lo, FLAG_HC);
        if(isFlagged(AF.lo, FLAG_C)){
            unflagBit(AF.lo, FLAG_C);
        }
        else{
            flagBit(AF.lo, FLAG_C);
        }
    }
    
    void CPU_SCF(){
        unflagBit(AF.lo, FLAG_N);
        unflagBit(AF.lo, FLAG_HC);
        flagBit(AF.lo, FLAG_C);
    }
    
    // NOP ignored because we'll handle clock cycle updates in the switch statement
    
    void CPU_HALT(){
        if(!IME && (ifRegister & ieRegister & 0x1F)){
            return;
        }
        PC--;
        halt = true;
    }
    
    void CPU_DI(){
        IME = false;
    }
    
    void CPU_EI(){
        IME = true;
    }
    
    // Jump Commands
    void CPU_JP(bool useFlag, const WORD& address, const BYTE& flag, bool set){
        if(!useFlag){
            PC = address;
        }
        else if(set && isFlagged(AF.lo, flag)){
            PC = address;
        }
        else if(!set && !isFlagged(AF.lo, flag)){
            PC = address;
        }
    }
    
    void CPU_JR(bool useFlag, const SIGNED_BYTE& address, const BYTE& flag, bool set){
        if(!useFlag){
            PC += address;
        }
        else if(set && isFlagged(AF.lo, flag)){
            PC += address;
        }
        else if(!set && !isFlagged(AF.lo, flag)){
            PC += address;
        }
    }
    
    void CPU_CALL(bool useFlag, const WORD& address, const BYTE& flag, bool set){
        if(!useFlag || (useFlag && set && isFlagged(AF.lo, flag)) || (useFlag && !set && !isFlagged(AF.lo, flag)) ){
            CPU_PUSH(PC);
            PC = address;
        }
    }
    
    void CPU_RET(bool useFlag, const BYTE& flag, bool set){
        if(!useFlag || (useFlag && set && isFlagged(AF.lo, flag)) || (useFlag && !set && !isFlagged(AF.lo, flag))){
            CPU_POP(PC);
        }
    }
    
    void CPU_RETI(){
        CPU_RET(0, 0, 0);
        IME = true;
    }
    
    void CPU_RST(const WORD& address){
        CPU_CALL(false, address, 0, 0);
    }
    
    void CPU_RESET(){
        AF.reg = 0;
        BC.reg = 0;
        DE.reg = 0;
        HL.reg = 0;
        SP.reg = 0;
        PC = 0;
        clock = 0;
        ifRegister = 0x0;
    }
    
    int executeExtendedOpcode(const BYTE& opcode){
        switch(opcode){
            case 0x00: CPU_RLC(BC.hi); return 8;
            case 0x01: CPU_RLC(BC.lo); return 8;
            case 0x02: CPU_RLC(DE.hi); return 8;
            case 0x03: CPU_RLC(DE.lo); return 8;
            case 0x04: CPU_RLC(HL.hi); return 8;
            case 0x05: CPU_RLC(HL.lo); return 8;
            case 0x07: CPU_RLC(AF.hi); return 8;
            case 0x06: CPU_RLC_WRITE(); return 16;
            case 0x10: CPU_RL(BC.hi); return 8;
            case 0x11: CPU_RL(BC.lo); return 8;
            case 0x12: CPU_RL(DE.hi); return 8;
            case 0x13: CPU_RL(DE.lo); return 8;
            case 0x14: CPU_RL(HL.hi); return 8;
            case 0x15: CPU_RL(HL.lo); return 8;
            case 0x17: CPU_RL(AF.hi); return 8;
            case 0x16: CPU_RL_WRITE(); return 16;
            case 0x08: CPU_RRC(BC.hi); return 8;
            case 0x09: CPU_RRC(BC.lo); return 8;
            case 0x0A: CPU_RRC(DE.hi); return 8;
            case 0x0B: CPU_RRC(DE.lo); return 8;
            case 0x0C: CPU_RRC(HL.hi); return 8;
            case 0x0D: CPU_RRC(HL.lo); return 8;
            case 0x0F: CPU_RRC(AF.hi); return 8;
            case 0x0E: CPU_RRC_WRITE(); return 16;
            case 0x18: CPU_RR(BC.hi); return 8;
            case 0x19: CPU_RR(BC.lo); return 8;
            case 0x1A: CPU_RR(DE.hi); return 8;
            case 0x1B: CPU_RR(DE.lo); return 8;
            case 0x1C: CPU_RR(HL.hi); return 8;
            case 0x1D: CPU_RR(HL.lo); return 8;
            case 0x1F: CPU_RR(AF.hi); return 8;
            case 0x1E: CPU_RR_WRITE(); return 16;
            case 0x20: CPU_SLA(BC.hi); return 8;
            case 0x21: CPU_SLA(BC.lo); return 8;
            case 0x22: CPU_SLA(DE.hi); return 8;
            case 0x23: CPU_SLA(DE.lo); return 8;
            case 0x24: CPU_SLA(HL.hi); return 8;
            case 0x25: CPU_SLA(HL.lo); return 8;
            case 0x27: CPU_SLA(AF.hi); return 8;
            case 0x26: CPU_SLA_WRITE(); return 16;
            case 0x30: CPU_SWAP(BC.hi); return 8;
            case 0x31: CPU_SWAP(BC.lo); return 8;
            case 0x32: CPU_SWAP(DE.hi); return 8;
            case 0x33: CPU_SWAP(DE.lo); return 8;
            case 0x34: CPU_SWAP(HL.hi); return 8;
            case 0x35: CPU_SWAP(HL.lo); return 8;
            case 0x37: CPU_SWAP(AF.hi); return 8;
            case 0x36: CPU_SWAP_WRITE(); return 16;
            case 0x28: CPU_SRA(BC.hi); return 8;
            case 0x29: CPU_SRA(BC.lo); return 8;
            case 0x2A: CPU_SRA(DE.hi); return 8;
            case 0x2B: CPU_SRA(DE.lo); return 8;
            case 0x2C: CPU_SRA(HL.hi); return 8;
            case 0x2D: CPU_SRA(HL.lo); return 8;
            case 0x2F: CPU_SRA(AF.hi); return 8;
            case 0x2E: CPU_SRA_WRITE(); return 16;
            case 0x38: CPU_SRL(BC.hi); return 8;
            case 0x39: CPU_SRL(BC.lo); return 8;
            case 0x3A: CPU_SRL(DE.hi); return 8;
            case 0x3B: CPU_SRL(DE.lo); return 8;
            case 0x3C: CPU_SRL(HL.hi); return 8;
            case 0x3D: CPU_SRL(HL.lo); return 8;
            case 0x3F: CPU_SRL(AF.hi); return 8;
            case 0x3E: CPU_SRL_WRITE(); return 16;
                // 1-Bit Operations
            case 0x40: CPU_BIT(0, BC.hi); return 8;
            case 0x41: CPU_BIT(0, BC.lo); return 8;
            case 0x42: CPU_BIT(0, DE.hi); return 8;
            case 0x43: CPU_BIT(0, DE.lo); return 8;
            case 0x44: CPU_BIT(0, HL.hi); return 8;
            case 0x45: CPU_BIT(0, HL.lo); return 8;
            case 0x47: CPU_BIT(0, AF.hi); return 8;
            case 0x48: CPU_BIT(1, BC.hi); return 8;
            case 0x49: CPU_BIT(1, BC.lo); return 8;
            case 0x4A: CPU_BIT(1, DE.hi); return 8;
            case 0x4B: CPU_BIT(1, DE.lo); return 8;
            case 0x4C: CPU_BIT(1, HL.hi); return 8;
            case 0x4D: CPU_BIT(1, HL.lo); return 8;
            case 0x4F: CPU_BIT(1, AF.hi); return 8;
            case 0x50: CPU_BIT(2, BC.hi); return 8;
            case 0x51: CPU_BIT(2, BC.lo); return 8;
            case 0x52: CPU_BIT(2, DE.hi); return 8;
            case 0x53: CPU_BIT(2, DE.lo); return 8;
            case 0x54: CPU_BIT(2, HL.hi); return 8;
            case 0x55: CPU_BIT(2, HL.lo); return 8;
            case 0x57: CPU_BIT(2, AF.hi); return 8;
            case 0x58: CPU_BIT(3, BC.hi); return 8;
            case 0x59: CPU_BIT(3, BC.lo); return 8;
            case 0x5A: CPU_BIT(3, DE.hi); return 8;
            case 0x5B: CPU_BIT(3, DE.lo); return 8;
            case 0x5C: CPU_BIT(3, HL.hi); return 8;
            case 0x5D: CPU_BIT(3, HL.lo); return 8;
            case 0x5F: CPU_BIT(3, AF.hi); return 8;
            case 0x60: CPU_BIT(4, BC.hi); return 8;
            case 0x61: CPU_BIT(4, BC.lo); return 8;
            case 0x62: CPU_BIT(4, DE.hi); return 8;
            case 0x63: CPU_BIT(4, DE.lo); return 8;
            case 0x64: CPU_BIT(4, HL.hi); return 8;
            case 0x65: CPU_BIT(4, HL.lo); return 8;
            case 0x67: CPU_BIT(4, AF.hi); return 8;
            case 0x68: CPU_BIT(5, BC.hi); return 8;
            case 0x69: CPU_BIT(5, BC.lo); return 8;
            case 0x6A: CPU_BIT(5, DE.hi); return 8;
            case 0x6B: CPU_BIT(5, DE.lo); return 8;
            case 0x6C: CPU_BIT(5, HL.hi); return 8;
            case 0x6D: CPU_BIT(5, HL.lo); return 8;
            case 0x6F: CPU_BIT(5, AF.hi); return 8;
            case 0x70: CPU_BIT(6, BC.hi); return 8;
            case 0x71: CPU_BIT(6, BC.lo); return 8;
            case 0x72: CPU_BIT(6, DE.hi); return 8;
            case 0x73: CPU_BIT(6, DE.lo); return 8;
            case 0x74: CPU_BIT(6, HL.hi); return 8;
            case 0x75: CPU_BIT(6, HL.lo); return 8;
            case 0x77: CPU_BIT(6, AF.hi); return 8;
            case 0x78: CPU_BIT(7, BC.hi); return 8;
            case 0x79: CPU_BIT(7, BC.lo); return 8;
            case 0x7A: CPU_BIT(7, DE.hi); return 8;
            case 0x7B: CPU_BIT(7, DE.lo); return 8;
            case 0x7C: CPU_BIT(7, HL.hi); return 8;
            case 0x7D: CPU_BIT(7, HL.lo); return 8;
            case 0x7F: CPU_BIT(7, AF.hi); return 8;
            case 0x46: CPU_BIT(0, mmu.readByte(HL.reg)); return 16;
            case 0x4E: CPU_BIT(1, mmu.readByte(HL.reg)); return 16;
            case 0x56: CPU_BIT(2, mmu.readByte(HL.reg)); return 16;
            case 0x5E: CPU_BIT(3, mmu.readByte(HL.reg)); return 16;
            case 0x66: CPU_BIT(4, mmu.readByte(HL.reg)); return 16;
            case 0x6E: CPU_BIT(5, mmu.readByte(HL.reg)); return 16;
            case 0x76: CPU_BIT(6, mmu.readByte(HL.reg)); return 16;
            case 0x7E: CPU_BIT(7, mmu.readByte(HL.reg)); return 16;
            case 0xC0: CPU_SET(0, BC.hi); return 8;
            case 0xC1: CPU_SET(0, BC.lo); return 8;
            case 0xC2: CPU_SET(0, DE.hi); return 8;
            case 0xC3: CPU_SET(0, DE.lo); return 8;
            case 0xC4: CPU_SET(0, HL.hi); return 8;
            case 0xC5: CPU_SET(0, HL.lo); return 8;
            case 0xC7: CPU_SET(0, AF.hi); return 8;
            case 0xC8: CPU_SET(1, BC.hi); return 8;
            case 0xC9: CPU_SET(1, BC.lo); return 8;
            case 0xCA: CPU_SET(1, DE.hi); return 8;
            case 0xCB: CPU_SET(1, DE.lo); return 8;
            case 0xCC: CPU_SET(1, HL.hi); return 8;
            case 0xCD: CPU_SET(1, HL.lo); return 8;
            case 0xCF: CPU_SET(1, AF.hi); return 8;
            case 0xD0: CPU_SET(2, BC.hi); return 8;
            case 0xD1: CPU_SET(2, BC.lo); return 8;
            case 0xD2: CPU_SET(2, DE.hi); return 8;
            case 0xD3: CPU_SET(2, DE.lo); return 8;
            case 0xD4: CPU_SET(2, HL.hi); return 8;
            case 0xD5: CPU_SET(2, HL.lo); return 8;
            case 0xD7: CPU_SET(2, AF.hi); return 8;
            case 0xD8: CPU_SET(3, BC.hi); return 8;
            case 0xD9: CPU_SET(3, BC.lo); return 8;
            case 0xDA: CPU_SET(3, DE.hi); return 8;
            case 0xDB: CPU_SET(3, DE.lo); return 8;
            case 0xDC: CPU_SET(3, HL.hi); return 8;
            case 0xDD: CPU_SET(3, HL.lo); return 8;
            case 0xDF: CPU_SET(3, AF.hi); return 8;
            case 0xE0: CPU_SET(4, BC.hi); return 8;
            case 0xE1: CPU_SET(4, BC.lo); return 8;
            case 0xE2: CPU_SET(4, DE.hi); return 8;
            case 0xE3: CPU_SET(4, DE.lo); return 8;
            case 0xE4: CPU_SET(4, HL.hi); return 8;
            case 0xE5: CPU_SET(4, HL.lo); return 8;
            case 0xE7: CPU_SET(4, AF.hi); return 8;
            case 0xE8: CPU_SET(5, BC.hi); return 8;
            case 0xE9: CPU_SET(5, BC.lo); return 8;
            case 0xEA: CPU_SET(5, DE.hi); return 8;
            case 0xEB: CPU_SET(5, DE.lo); return 8;
            case 0xEC: CPU_SET(5, HL.hi); return 8;
            case 0xED: CPU_SET(5, HL.lo); return 8;
            case 0xEF: CPU_SET(5, AF.hi); return 8;
            case 0xF0: CPU_SET(6, BC.hi); return 8;
            case 0xF1: CPU_SET(6, BC.lo); return 8;
            case 0xF2: CPU_SET(6, DE.hi); return 8;
            case 0xF3: CPU_SET(6, DE.lo); return 8;
            case 0xF4: CPU_SET(6, HL.hi); return 8;
            case 0xF5: CPU_SET(6, HL.lo); return 8;
            case 0xF7: CPU_SET(6, AF.hi); return 8;
            case 0xF8: CPU_SET(7, BC.hi); return 8;
            case 0xF9: CPU_SET(7, BC.lo); return 8;
            case 0xFA: CPU_SET(7, DE.hi); return 8;
            case 0xFB: CPU_SET(7, DE.lo); return 8;
            case 0xFC: CPU_SET(7, HL.hi); return 8;
            case 0xFD: CPU_SET(7, HL.lo); return 8;
            case 0xFF: CPU_SET(7, AF.hi); return 8;
            case 0xC6: CPU_SET_WRITE(0); return 16;
            case 0xCE: CPU_SET_WRITE(1); return 16;
            case 0xD6: CPU_SET_WRITE(2); return 16;
            case 0xDE: CPU_SET_WRITE(3); return 16;
            case 0xE6: CPU_SET_WRITE(4); return 16;
            case 0xEE: CPU_SET_WRITE(5); return 16;
            case 0xF6: CPU_SET_WRITE(6); return 16;
            case 0xFE: CPU_SET_WRITE(7); return 16;
            case 0x80: CPU_RES(0, BC.hi); return 8;
            case 0x81: CPU_RES(0, BC.lo); return 8;
            case 0x82: CPU_RES(0, DE.hi); return 8;
            case 0x83: CPU_RES(0, DE.lo); return 8;
            case 0x84: CPU_RES(0, HL.hi); return 8;
            case 0x85: CPU_RES(0, HL.lo); return 8;
            case 0x87: CPU_RES(0, AF.hi); return 8;
            case 0x88: CPU_RES(1, BC.hi); return 8;
            case 0x89: CPU_RES(1, BC.lo); return 8;
            case 0x8A: CPU_RES(1, DE.hi); return 8;
            case 0x8B: CPU_RES(1, DE.lo); return 8;
            case 0x8C: CPU_RES(1, HL.hi); return 8;
            case 0x8D: CPU_RES(1, HL.lo); return 8;
            case 0x8F: CPU_RES(1, AF.hi); return 8;
            case 0x90: CPU_RES(2, BC.hi); return 8;
            case 0x91: CPU_RES(2, BC.lo); return 8;
            case 0x92: CPU_RES(2, DE.hi); return 8;
            case 0x93: CPU_RES(2, DE.lo); return 8;
            case 0x94: CPU_RES(2, HL.hi); return 8;
            case 0x95: CPU_RES(2, HL.lo); return 8;
            case 0x97: CPU_RES(2, AF.hi); return 8;
            case 0x98: CPU_RES(3, BC.hi); return 8;
            case 0x99: CPU_RES(3, BC.lo); return 8;
            case 0x9A: CPU_RES(3, DE.hi); return 8;
            case 0x9B: CPU_RES(3, DE.lo); return 8;
            case 0x9C: CPU_RES(3, HL.hi); return 8;
            case 0x9D: CPU_RES(3, HL.lo); return 8;
            case 0x9F: CPU_RES(3, AF.hi); return 8;
            case 0xA0: CPU_RES(4, BC.hi); return 8;
            case 0xA1: CPU_RES(4, BC.lo); return 8;
            case 0xA2: CPU_RES(4, DE.hi); return 8;
            case 0xA3: CPU_RES(4, DE.lo); return 8;
            case 0xA4: CPU_RES(4, HL.hi); return 8;
            case 0xA5: CPU_RES(4, HL.lo); return 8;
            case 0xA7: CPU_RES(4, AF.hi); return 8;
            case 0xA8: CPU_RES(5, BC.hi); return 8;
            case 0xA9: CPU_RES(5, BC.lo); return 8;
            case 0xAA: CPU_RES(5, DE.hi); return 8;
            case 0xAB: CPU_RES(5, DE.lo); return 8;
            case 0xAC: CPU_RES(5, HL.hi); return 8;
            case 0xAD: CPU_RES(5, HL.lo); return 8;
            case 0xAF: CPU_RES(5, AF.hi); return 8;
            case 0xB0: CPU_RES(6, BC.hi); return 8;
            case 0xB1: CPU_RES(6, BC.lo); return 8;
            case 0xB2: CPU_RES(6, DE.hi); return 8;
            case 0xB3: CPU_RES(6, DE.lo); return 8;
            case 0xB4: CPU_RES(6, HL.hi); return 8;
            case 0xB5: CPU_RES(6, HL.lo); return 8;
            case 0xB7: CPU_RES(6, AF.hi); return 8;
            case 0xB8: CPU_RES(7, BC.hi); return 8;
            case 0xB9: CPU_RES(7, BC.lo); return 8;
            case 0xBA: CPU_RES(7, DE.hi); return 8;
            case 0xBB: CPU_RES(7, DE.lo); return 8;
            case 0xBC: CPU_RES(7, HL.hi); return 8;
            case 0xBD: CPU_RES(7, HL.lo); return 8;
            case 0xBF: CPU_RES(7, AF.hi); return 8;
            case 0x86: CPU_RES_WRITE(0); return 16;
            case 0x8E: CPU_RES_WRITE(1); return 16;
            case 0x96: CPU_RES_WRITE(2); return 16;
            case 0x9E: CPU_RES_WRITE(3); return 16;
            case 0xA6: CPU_RES_WRITE(4); return 16;
            case 0xAE: CPU_RES_WRITE(5); return 16;
            case 0xB6: CPU_RES_WRITE(6); return 16;
            case 0xBE: CPU_RES_WRITE(7); return 16;
            default: assert(false); return 0;
        }
    }
    
    int executeOpcode(const BYTE& opcode){
        //std::cout << "OpCode: " << std::hex << (int) opcode << std::endl;
        switch(opcode){
                // 8-Bit Loads
            case 0x78: CPU_LOAD(AF.hi, BC.hi); return 4;
            case 0x79: CPU_LOAD(AF.hi, BC.lo); return 4;
            case 0x7A: CPU_LOAD(AF.hi, DE.hi); return 4;
            case 0x7B: CPU_LOAD(AF.hi, DE.lo); return 4;
            case 0x7C: CPU_LOAD(AF.hi, HL.hi); return 4;
            case 0x7D: CPU_LOAD(AF.hi, HL.lo); return 4;
            case 0x7F: CPU_LOAD(AF.hi, AF.hi); return 4;
            case 0x40: CPU_LOAD(BC.hi, BC.hi); return 4;
            case 0x41: CPU_LOAD(BC.hi, BC.lo); return 4;
            case 0x42: CPU_LOAD(BC.hi, DE.hi); return 4;
            case 0x43: CPU_LOAD(BC.hi, DE.lo); return 4;
            case 0x44: CPU_LOAD(BC.hi, HL.hi); return 4;
            case 0x45: CPU_LOAD(BC.hi, HL.lo); return 4;
            case 0x47: CPU_LOAD(BC.hi, AF.hi); return 4;
            case 0x48: CPU_LOAD(BC.lo, BC.hi); return 4;
            case 0x49: CPU_LOAD(BC.lo, BC.lo); return 4;
            case 0x4A: CPU_LOAD(BC.lo, DE.hi); return 4;
            case 0x4B: CPU_LOAD(BC.lo, DE.lo); return 4;
            case 0x4C: CPU_LOAD(BC.lo, HL.hi); return 4;
            case 0x4D: CPU_LOAD(BC.lo, HL.lo); return 4;
            case 0x4F: CPU_LOAD(BC.lo, AF.hi); return 4;
            case 0x50: CPU_LOAD(DE.hi, BC.hi); return 4;
            case 0x51: CPU_LOAD(DE.hi, BC.lo); return 4;
            case 0x52: CPU_LOAD(DE.hi, DE.hi); return 4;
            case 0x53: CPU_LOAD(DE.hi, DE.lo); return 4;
            case 0x54: CPU_LOAD(DE.hi, HL.hi); return 4;
            case 0x55: CPU_LOAD(DE.hi, HL.lo); return 4;
            case 0x57: CPU_LOAD(DE.hi, AF.hi); return 4;
            case 0x58: CPU_LOAD(DE.lo, BC.hi); return 4;
            case 0x59: CPU_LOAD(DE.lo, BC.lo); return 4;
            case 0x5A: CPU_LOAD(DE.lo, DE.hi); return 4;
            case 0x5B: CPU_LOAD(DE.lo, DE.lo); return 4;
            case 0x5C: CPU_LOAD(DE.lo, HL.hi); return 4;
            case 0x5D: CPU_LOAD(DE.lo, HL.lo); return 4;
            case 0x5F: CPU_LOAD(DE.lo, AF.hi); return 4;
            case 0x60: CPU_LOAD(HL.hi, BC.hi); return 4;
            case 0x61: CPU_LOAD(HL.hi, BC.lo); return 4;
            case 0x62: CPU_LOAD(HL.hi, DE.hi); return 4;
            case 0x63: CPU_LOAD(HL.hi, DE.lo); return 4;
            case 0x64: CPU_LOAD(HL.hi, HL.hi); return 4;
            case 0x65: CPU_LOAD(HL.hi, HL.lo); return 4;
            case 0x67: CPU_LOAD(HL.hi, AF.hi); return 4;
            case 0x68: CPU_LOAD(HL.lo, BC.hi); return 4;
            case 0x69: CPU_LOAD(HL.lo, BC.lo); return 4;
            case 0x6A: CPU_LOAD(HL.lo, DE.hi); return 4;
            case 0x6B: CPU_LOAD(HL.lo, DE.lo); return 4;
            case 0x6C: CPU_LOAD(HL.lo, HL.hi); return 4;
            case 0x6D: CPU_LOAD(HL.lo, HL.lo); return 4;
            case 0x6F: CPU_LOAD(HL.lo, AF.hi); return 4;
            case 0x3E: CPU_LOAD(AF.hi, mmu.readByte(PC++)); return 8;
            case 0x06: CPU_LOAD(BC.hi, mmu.readByte(PC++)); return 8;
            case 0x0E: CPU_LOAD(BC.lo, mmu.readByte(PC++)); return 8;
            case 0x16: CPU_LOAD(DE.hi, mmu.readByte(PC++)); return 8;
            case 0x1E: CPU_LOAD(DE.lo, mmu.readByte(PC++)); return 8;
            case 0x26: CPU_LOAD(HL.hi, mmu.readByte(PC++)); return 8;
            case 0x2E: CPU_LOAD(HL.lo, mmu.readByte(PC++)); return 8;
            case 0x7E: CPU_LOAD(AF.hi, mmu.readByte(HL.reg)); return 8;
            case 0x46: CPU_LOAD(BC.hi, mmu.readByte(HL.reg)); return 8;
            case 0x4E: CPU_LOAD(BC.lo, mmu.readByte(HL.reg)); return 8;
            case 0x56: CPU_LOAD(DE.hi, mmu.readByte(HL.reg)); return 8;
            case 0x5E: CPU_LOAD(DE.lo, mmu.readByte(HL.reg)); return 8;
            case 0x66: CPU_LOAD(HL.hi, mmu.readByte(HL.reg)); return 8;
            case 0x6E: CPU_LOAD(HL.lo, mmu.readByte(HL.reg)); return 8;
            case 0x70: CPU_LOAD_WRITE(HL.reg, BC.hi); return 8;
            case 0x71: CPU_LOAD_WRITE(HL.reg, BC.lo); return 8;
            case 0x72: CPU_LOAD_WRITE(HL.reg, DE.hi); return 8;
            case 0x73: CPU_LOAD_WRITE(HL.reg, DE.lo); return 8;
            case 0x74: CPU_LOAD_WRITE(HL.reg, HL.hi); return 8;
            case 0x75: CPU_LOAD_WRITE(HL.reg, HL.lo); return 8;
            case 0x77: CPU_LOAD_WRITE(HL.reg, AF.hi); return 8;
            case 0x36: CPU_LOAD_WRITE(HL.reg, mmu.readByte(PC++)); return 12;
            case 0x0A: CPU_LOAD(AF.hi, mmu.readByte(BC.reg)); return 8;
            case 0x1A: CPU_LOAD(AF.hi, mmu.readByte(DE.reg)); return 8;
            case 0xFA: PC += 2; CPU_LOAD(AF.hi, mmu.readByte(mmu.readWord(PC - 2))); return 16;
            case 0x02: CPU_LOAD_WRITE(BC.reg, AF.hi); return 8;
            case 0x12: CPU_LOAD_WRITE(DE.reg, AF.hi); return 8;
            case 0xEA: PC += 2; CPU_LOAD_WRITE(mmu.readWord(PC - 2), AF.hi); return 16;
            case 0x08: PC += 2; CPU_LOAD_WRITE_16BIT(mmu.readWord(PC - 2), SP.reg); return 20;
            case 0xF0: CPU_LOAD(AF.hi, mmu.readByte(0xFF00 + mmu.readByte(PC++))); return 12;
            case 0xE0: CPU_LOAD_WRITE(0xFF00 + mmu.readByte(PC++), AF.hi); return 12;
            case 0xF2: CPU_LOAD(AF.hi, mmu.readByte(0xFF00 + BC.lo)); return 8;
            case 0xE2: CPU_LOAD_WRITE(0xFF00 + BC.lo, AF.hi); return 8;
            case 0x22: CPU_LOAD_WRITE(HL.reg++, AF.hi); return 8;
            case 0x2A: CPU_LOAD(AF.hi, mmu.readByte(HL.reg++)); return 8;
            case 0x32: CPU_LOAD_WRITE(HL.reg--, AF.hi); return 8;
            case 0x3A: CPU_LOAD(AF.hi, mmu.readByte(HL.reg--)); return 8;
                // 16-Bit Loads
            case 0x01: PC += 2; CPU_LOAD_16BIT(BC.reg, mmu.readWord(PC - 2), 0, false); return 12;
            case 0x11: PC += 2; CPU_LOAD_16BIT(DE.reg, mmu.readWord(PC - 2), 0,false); return 12;
            case 0x21: PC += 2; CPU_LOAD_16BIT(HL.reg, mmu.readWord(PC - 2), 0, false); return 12;
            case 0x31: PC += 2; CPU_LOAD_16BIT(SP.reg, mmu.readWord(PC - 2), 0, false); return 12;
            case 0xF9: CPU_LOAD_16BIT(SP.reg, HL.reg, 0, false); return 8;
            case 0xC5: CPU_PUSH(BC.reg); return 16;
            case 0xD5: CPU_PUSH(DE.reg); return 16;
            case 0xE5: CPU_PUSH(HL.reg); return 16;
            case 0xF5: AF.reg &= 0xFFF0; CPU_PUSH(AF.reg); return 16;
            case 0xC1: CPU_POP(BC.reg); return 12;
            case 0xD1: CPU_POP(DE.reg); return 12;
            case 0xE1: CPU_POP(HL.reg); return 12;
            case 0xF1: AF.reg &= 0xFFF0; CPU_POP(AF.reg); return 12;
                // 8-Bit Arithmetic
            case 0x80: CPU_ADD(BC.hi, false); return 4;
            case 0x81: CPU_ADD(BC.lo, false); return 4;
            case 0x82: CPU_ADD(DE.hi, false); return 4;
            case 0x83: CPU_ADD(DE.lo, false); return 4;
            case 0x84: CPU_ADD(HL.hi, false); return 4;
            case 0x85: CPU_ADD(HL.lo, false); return 4;
            case 0x87: CPU_ADD(AF.hi, false); return 4;
            case 0xC6: CPU_ADD(mmu.readByte(PC++), false); return 8;
            case 0x86: CPU_ADD(mmu.readByte(HL.reg), false); return 8;
            case 0x88: CPU_ADD(BC.hi, true); return 4;
            case 0x89: CPU_ADD(BC.lo, true); return 4;
            case 0x8A: CPU_ADD(DE.hi, true); return 4;
            case 0x8B: CPU_ADD(DE.lo, true); return 4;
            case 0x8C: CPU_ADD(HL.hi, true); return 4;
            case 0x8D: CPU_ADD(HL.lo, true); return 4;
            case 0x8F: CPU_ADD(AF.hi, true); return 4;
            case 0xCE: CPU_ADD(mmu.readByte(PC++), true); return 8;
            case 0x8E: CPU_ADD(mmu.readByte(HL.reg), true); return 8;
            case 0x90: CPU_SUB(BC.hi, false); return 4;
            case 0x91: CPU_SUB(BC.lo, false); return 4;
            case 0x92: CPU_SUB(DE.hi, false); return 4;
            case 0x93: CPU_SUB(DE.lo, false); return 4;
            case 0x94: CPU_SUB(HL.hi, false); return 4;
            case 0x95: CPU_SUB(HL.lo, false); return 4;
            case 0x97: CPU_SUB(AF.hi, false); return 4;
            case 0xD6: CPU_SUB(mmu.readByte(PC++), false); return 8;
            case 0x96: CPU_SUB(mmu.readByte(HL.reg), false); return 8;
            case 0x98: CPU_SUB(BC.hi, true); return 4;
            case 0x99: CPU_SUB(BC.lo, true); return 4;
            case 0x9A: CPU_SUB(DE.hi, true); return 4;
            case 0x9B: CPU_SUB(DE.lo, true); return 4;
            case 0x9C: CPU_SUB(HL.hi, true); return 4;
            case 0x9D: CPU_SUB(HL.lo, true); return 4;
            case 0x9F: CPU_SUB(AF.hi, true); return 4;
            case 0xDE: CPU_SUB(mmu.readByte(PC++), true); return 8;
            case 0x9E: CPU_SUB(mmu.readByte(HL.reg), true); return 8;
            case 0xA0: CPU_AND(BC.hi); return 4;
            case 0xA1: CPU_AND(BC.lo); return 4;
            case 0xA2: CPU_AND(DE.hi); return 4;
            case 0xA3: CPU_AND(DE.lo); return 4;
            case 0xA4: CPU_AND(HL.hi); return 4;
            case 0xA5: CPU_AND(HL.lo); return 4;
            case 0xA7: CPU_AND(AF.hi); return 4;
            case 0xE6: CPU_AND(mmu.readByte(PC++)); return 8;
            case 0xA6: CPU_AND(mmu.readByte(HL.reg)); return 8;
            case 0xA8: CPU_XOR(BC.hi); return 4;
            case 0xA9: CPU_XOR(BC.lo); return 4;
            case 0xAA: CPU_XOR(DE.hi); return 4;
            case 0xAB: CPU_XOR(DE.lo); return 4;
            case 0xAC: CPU_XOR(HL.hi); return 4;
            case 0xAD: CPU_XOR(HL.lo); return 4;
            case 0xAF: CPU_XOR(AF.hi); return 4;
            case 0xEE: CPU_XOR(mmu.readByte(PC++)); return 8;
            case 0xAE: CPU_XOR(mmu.readByte(HL.reg)); return 8;
            case 0xB0: CPU_OR(BC.hi); return 4;
            case 0xB1: CPU_OR(BC.lo); return 4;
            case 0xB2: CPU_OR(DE.hi); return 4;
            case 0xB3: CPU_OR(DE.lo); return 4;
            case 0xB4: CPU_OR(HL.hi); return 4;
            case 0xB5: CPU_OR(HL.lo); return 4;
            case 0xB7: CPU_OR(AF.hi); return 4;
            case 0xF6: CPU_OR(mmu.readByte(PC++)); return 8;
            case 0xB6: CPU_OR(mmu.readByte(HL.reg)); return 8;
            case 0xB8: CPU_CP(BC.hi); return 4;
            case 0xB9: CPU_CP(BC.lo); return 4;
            case 0xBA: CPU_CP(DE.hi); return 4;
            case 0xBB: CPU_CP(DE.lo); return 4;
            case 0xBC: CPU_CP(HL.hi); return 4;
            case 0xBD: CPU_CP(HL.lo); return 4;
            case 0xBF: CPU_CP(AF.hi); return 4;
            case 0xFE: CPU_CP(mmu.readByte(PC++)); return 8;
            case 0xBE: CPU_CP(mmu.readByte(HL.reg)); return 8;
            case 0x04: CPU_INC(BC.hi); return 4;
            case 0x0C: CPU_INC(BC.lo); return 4;
            case 0x14: CPU_INC(DE.hi); return 4;
            case 0x1C: CPU_INC(DE.lo); return 4;
            case 0x24: CPU_INC(HL.hi); return 4;
            case 0x2C: CPU_INC(HL.lo); return 4;
            case 0x3C: CPU_INC(AF.hi); return 4;
            case 0x34: CPU_INC_WRITE(); return 12;
            case 0x05: CPU_DEC(BC.hi); return 4;
            case 0x0D: CPU_DEC(BC.lo); return 4;
            case 0x15: CPU_DEC(DE.hi); return 4;
            case 0x1D: CPU_DEC(DE.lo); return 4;
            case 0x25: CPU_DEC(HL.hi); return 4;
            case 0x2D: CPU_DEC(HL.lo); return 4;
            case 0x3D: CPU_DEC(AF.hi); return 4;
            case 0x35: CPU_DEC_WRITE(); return 12;
            case 0x27: CPU_DAA(); return 4;
            case 0x2F: CPU_CPL(); return 4;
                // 16-Bit Arithmetic/Logical Commands
            case 0x09: CPU_ADD_16BIT(HL.reg, BC.reg); return 8;
            case 0x19: CPU_ADD_16BIT(HL.reg, DE.reg); return 8;
            case 0x29: CPU_ADD_16BIT(HL.reg, HL.reg); return 8;
            case 0x39: CPU_ADD_16BIT(HL.reg, SP.reg); return 8;
            case 0x03: CPU_INC_16BIT(BC.reg); return 8;
            case 0x13: CPU_INC_16BIT(DE.reg); return 8;
            case 0x23: CPU_INC_16BIT(HL.reg); return 8;
            case 0x33: CPU_INC_16BIT(SP.reg); return 8;
            case 0x0B: CPU_DEC_16BIT(BC.reg); return 8;
            case 0x1B: CPU_DEC_16BIT(DE.reg); return 8;
            case 0x2B: CPU_DEC_16BIT(HL.reg); return 8;
            case 0x3B: CPU_DEC_16BIT(SP.reg); return 8;
            case 0xE8: CPU_ADD_16BIT_SIGNED(SP.reg, (SIGNED_BYTE) mmu.readByte(PC++)); return 16;
            case 0xF8: CPU_LOAD_16BIT(HL.reg, SP.reg, (SIGNED_BYTE) mmu.readByte(PC++), true); return 12;
                // Rotate and Shift Commands
            case 0x07: CPU_RLCA(AF.hi); return 4;
            case 0x17: CPU_RLA(AF.hi); return 4;
            case 0x0F: CPU_RRCA(AF.hi); return 4;
            case 0x1F: CPU_RRA(AF.hi); return 4;
                // Includes the rotate/shift + 1-bit operations
            case 0xCB: return executeExtendedOpcode(mmu.readByte(PC++));
                // CPU-Control Commands
            case 0x3F: CPU_CCF(); return 4;
            case 0x37: CPU_SCF(); return 4;
            case 0x00: return 4;
            case 0x76: CPU_HALT(); if(!halt){return 4 + executeOpcode(PC);} return 4;
            case 0x10: return 4;
            case 0xF3: CPU_DI(); return 4;
            case 0xFB: CPU_EI(); return 4;
                // Jump Commands
            case 0xC3: PC += 2; CPU_JP(0, mmu.readWord(PC - 2), 0, 0); return 16;
            case 0xE9: CPU_JP(0, HL.reg, 0, 0); return 4;
            case 0xC2: PC += 2; CPU_JP(1, mmu.readWord(PC - 2), FLAG_Z, 0); return !isFlagged(AF.lo, FLAG_Z) ? 16 : 12;
            case 0xCA: PC += 2; CPU_JP(1, mmu.readWord(PC - 2), FLAG_Z, 1); return isFlagged(AF.lo, FLAG_Z) ? 16 : 12;
            case 0xD2: PC += 2; CPU_JP(1, mmu.readWord(PC - 2), FLAG_C, 0); return !isFlagged(AF.lo, FLAG_C) ? 16 : 12;
            case 0xDA: PC += 2; CPU_JP(1, mmu.readWord(PC - 2), FLAG_C, 1); return isFlagged(AF.lo, FLAG_C) ? 16 : 12;
            case 0x18: CPU_JR(0, (SIGNED_BYTE) mmu.readByte(PC++), 0, 0); return 12;
            case 0x20: CPU_JR(1, (SIGNED_BYTE) mmu.readByte(PC++), FLAG_Z, 0); return !isFlagged(AF.lo, FLAG_Z) ? 12 : 8;
            case 0x28: CPU_JR(1, (SIGNED_BYTE) mmu.readByte(PC++), FLAG_Z, 1); return isFlagged(AF.lo, FLAG_Z) ? 12 : 8;
            case 0x30: CPU_JR(1, (SIGNED_BYTE) mmu.readByte(PC++), FLAG_C, 0); return !isFlagged(AF.lo, FLAG_C) ? 12 : 8;
            case 0x38: CPU_JR(1, (SIGNED_BYTE) mmu.readByte(PC++), FLAG_C, 1); return isFlagged(AF.lo, FLAG_C) ? 12 : 8;
            case 0xCD: PC += 2; CPU_CALL(0, mmu.readWord(PC - 2), 0, 0); return 24;
            case 0xC4: PC += 2; CPU_CALL(1, mmu.readWord(PC - 2), FLAG_Z, 0); return !isFlagged(AF.lo, FLAG_Z) ? 24 : 12;
            case 0xCC: PC += 2; CPU_CALL(1, mmu.readWord(PC - 2), FLAG_Z, 1); return isFlagged(AF.lo, FLAG_Z) ? 24 : 12;
            case 0xD4: PC += 2; CPU_CALL(1, mmu.readWord(PC - 2), FLAG_C, 0); return !isFlagged(AF.lo, FLAG_C) ? 24 : 12;
            case 0xDC: PC += 2; CPU_CALL(1, mmu.readWord(PC - 2), FLAG_C, 1); return isFlagged(AF.lo, FLAG_C) ? 24 : 12;
            case 0xC9: CPU_RET(0, 0, 0); return 16;
            case 0xC0: CPU_RET(1, FLAG_Z, 0); return !isFlagged(AF.lo, FLAG_Z) ? 20 : 8;
            case 0xC8: CPU_RET(1, FLAG_Z, 1); return isFlagged(AF.lo, FLAG_Z) ? 20 : 8;
            case 0xD0: CPU_RET(1, FLAG_C, 0); return !isFlagged(AF.lo, FLAG_C) ? 20 : 8;
            case 0xD8: CPU_RET(1, FLAG_C, 1); return isFlagged(AF.lo, FLAG_C) ? 20 : 8;
            case 0xD9: CPU_RETI(); return 16;
            case 0xC7: CPU_RST(0x0000); return 16;
            case 0xCF: CPU_RST(0x0008); return 16;
            case 0xD7: CPU_RST(0x0010); return 16;
            case 0xDF: CPU_RST(0x0018); return 16;
            case 0xE7: CPU_RST(0x0020); return 16;
            case 0xEF: CPU_RST(0x0028); return 16;
            case 0xF7: CPU_RST(0x0030); return 16;
            case 0xFF: CPU_RST(0x0038); return 16;
            default: assert(false); return 0;
        }
    }
    
public:
    
    void reset(){
        CPU_RESET();
    }
    
    void addToClock(int clockCycles){
        clock += clockCycles;
    }
    
    void handleInterrupts(){
        
        // Check what interrupts are enabled by using the IE and IF registers respectively
        BYTE interrupts =  ifRegister & ieRegister & 0x1F;
        
        if(halt && interrupts != 0){
            PC++;
            halt = false;
        }
        
        if(!IME){
            return;
        }
        
        // V-blank interrupt
        if(interrupts & 0x1){
            IME = false;
            ifRegister &= 0xFE;
            CPU_RST(0x0040);
        }
        // LCD STAT
        else if(interrupts & 0x2){
            IME = false;
            ifRegister &= 0xFD;
            CPU_RST(0x0048);
        }
        // Timer
        else if(interrupts & 0x4){
            IME = false;
            ifRegister &= 0xFB;
            CPU_RST(0x0050);
        }
        // Joypad
        else if(interrupts & 0x10){
            IME = false;
            ifRegister &= 0xEF;
            CPU_RST(0x0060);
        }
    }
    
    int step(){
        return executeOpcode(mmu.readByte(PC++));
    }
    
};

CPU cpu;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void printTileSet();
class PPU{
    
    int mode = 2;
    int clock = 0;
    
    SDL_Window *window;
    SDL_Renderer *renderer;
    
    // width * height * (r, g, b, a) where each of {r, g, b, a} is a byte value
    BYTE frameBuffer[FRAME_BUFFER_LENGTH];
    
    void initTileSet(){
        for(int tile = 0; tile < MAX_TILES; tile++){
            for(int y = 0; y < 8; y++){
                for(int x = 0; x < 8; x++){
                    mmu.tileSet[tile][y][x] = 0;
                }
            }
        }
    }
    
    void initSpriteSet(){
        for(int i = 0, addr = 0xFE00; i < 40; i++, addr+=4){
            mmu.writeByte(addr, 0);
            mmu.writeByte(addr + 1, 0);
            mmu.writeByte(addr + 2, 0);
            mmu.writeByte(addr + 3, 0);
        }
    }
    
    void initVideo(){
        SDL_Init(SDL_INIT_VIDEO);
        // width, height, flags, window, renderer
        //SDL_CreateWindowAndRenderer(160, 144, 0, &window, &renderer);
        window = SDL_CreateWindow("Gameboy", SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED, 160, 144, SDL_WINDOW_SHOWN);
        // Create renderer for window
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }
    
    void renderImage(){
        for(int y = 0; y < 144; y++){
            for(int x = 0; x < 160; x++){
                int pixel = (y * 160 + x) * 4;
                SDL_SetRenderDrawColor(renderer, frameBuffer[pixel], frameBuffer[pixel + 1], frameBuffer[pixel + 2], frameBuffer[pixel + 3]);
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }
        SDL_RenderPresent(renderer);
    }
    
    void renderBackground(BYTE scanRow[160]){
        // Determine which map to use
        WORD mapOffset = mmu.bgMap ? 0x9C00 : 0x9800;
        
        // Work out the index of the pixel in the framebuffer
        WORD lineOffset = mmu.scrollX % 256;
        WORD rowOffset = (mmu.scrollY + mmu.line) % 256;
        
        // Work out the tile for this pixel
        WORD tileX = lineOffset / 8;
        WORD tileY = rowOffset / 8;
        
        // Work out the index of the tile in the array of all tiles
        WORD tileIndex = tileY * 32 + tileX;
        WORD tileIDAddress = mapOffset + tileIndex;
        
        // Narrow down exactly which row of pixels and offset to start from
        int y = (mmu.line + mmu.scrollY) & 0x7;
        int x = mmu.scrollX & 0x7;
        
        // Determine where to draw on screen (framebuffer)
        int screenOffset = mmu.line * 160 * 4;
        
        int tile = mmu.readByte(tileIDAddress);
        
        if(!mmu.bgTile && tile < 128){
            tile += 256;
        }
        
        for(int column = 0; column < 160; column++){
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
            
            // Read next tile
            x++;
            if(x == 8){
                x = 0;
                tileIndex++;
                tile = mmu.readByte(mapOffset + tileIndex);
                if(!mmu.bgTile && tile < 128){
                    tile += 256;
                }
            }
        }
    }
    
    void renderSprites(BYTE scanRow[160]){
        for(int i = 0; i < 40; i++){
            
            SPRITE& sprite = mmu.spriteSet[i];
            
            if(sprite.getPosY() <= mmu.line && (sprite.getPosY() + 8) > mmu.line){
                int screenOffset = (mmu.line * 160 + sprite.getPosX()) * 4;
                
                BYTE tileRow[8];
                for(int j = 0; j < 8; j++){
                    tileRow[j] = mmu.tileSet[sprite.getTileNumber()]
                                            [sprite.isFlippedY() ?
                                             (7 - (mmu.line - sprite.getPosY())) :
                                             (mmu.line - sprite.getPosY())
                                            ]
                                            [j];
                }
                
                BYTE palette[4][4];
                for(int j = 0; j < 4; j++){
                    for(int k = 0; k < 4; k++){
                        palette[j][k] = sprite.isZeroPalette() ? mmu.obj0Palette[j][k] : mmu.obj1Palette[j][k];
                    }
                }
                
                BYTE colour[4];
                for(int x = 0; x < 8; x++){
                    if((sprite.getPosX() + x) >= 0 && (sprite.getPosX() + x) < 160 && (tileRow[sprite.isFlippedX() ? (7 - x) : x]) && (sprite.isPrioritized() || !scanRow[sprite.getPosX() + x])){

                        for(int j = 0; j < 4; j++){
                            colour[j] = palette[tileRow[sprite.isFlippedX() ? (7 - x) : x]][j];
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
    
    void renderScan(){
        if(!mmu.switchLCD){
            return;
        }
        BYTE scanRow[160];
        if(mmu.switchBG){
            renderBackground(scanRow);
        }
        if(mmu.switchOBJ){
            renderSprites(scanRow);
        }
    }
    
    void setLCDStatus(){
//        if(!mmu.switchLCD){
//            clock = 1824;
//            mmu.line = 0;
//            mmu.lcdStatRegister &= 0xFC;
//            mmu.lcdStatRegister |= 0x01;
//            return;
//        }
        
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
    
public:
    
    void reset(){
        initTileSet();
        initSpriteSet();
        initVideo();
    }
    
    void step(){
        
        setLCDStatus();
        
//        if(!mmu.switchLCD){
//            return;
//        }
        
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
    
    void quit(){
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
    
    void addToClock(int clockCycles){
        clock += clockCycles;
    }
};

PPU ppu;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Disassembler for debugging purposes

void disassembleExtendedOpcode(const BYTE& opcode){
    switch(opcode){
        case 0x00: std::cout << "RLC B"; break;
        case 0x01: std::cout << "RLC C"; break;
        case 0x02: std::cout << "RLC D"; break;
        case 0x03: std::cout << "RLC E"; break;
        case 0x04: std::cout << "RLC H"; break;
        case 0x05: std::cout << "RLC L"; break;
        case 0x07: std::cout << "RLC A"; break;
        case 0x06: std::cout << "RLC (HL)"; break;
        case 0x10: std::cout << "RL B"; break;
        case 0x11: std::cout << "RL C"; break;
        case 0x12: std::cout << "RL D"; break;
        case 0x13: std::cout << "RL E"; break;
        case 0x14: std::cout << "RL H"; break;
        case 0x15: std::cout << "RL L"; break;
        case 0x17: std::cout << "RL A"; break;
        case 0x16: std::cout << "RL (HL)"; break;
        case 0x08: std::cout << "RRC B"; break;
        case 0x09: std::cout << "RRC C"; break;
        case 0x0A: std::cout << "RRC D"; break;
        case 0x0B: std::cout << "RRC E"; break;
        case 0x0C: std::cout << "RRC H"; break;
        case 0x0D: std::cout << "RRC L"; break;
        case 0x0F: std::cout << "RRC A"; break;
        case 0x0E: std::cout << "RRC (HL)"; break;
        case 0x18: std::cout << "RR B"; break;
        case 0x19: std::cout << "RR C"; break;
        case 0x1A: std::cout << "RR D"; break;
        case 0x1B: std::cout << "RR E"; break;
        case 0x1C: std::cout << "RR H"; break;
        case 0x1D: std::cout << "RR L"; break;
        case 0x1F: std::cout << "RR A"; break;
        case 0x1E: std::cout << "RR (HL)"; break;
        case 0x20: std::cout << "SLA B"; break;
        case 0x21: std::cout << "SLA C"; break;
        case 0x22: std::cout << "SLA D"; break;
        case 0x23: std::cout << "SLA E"; break;
        case 0x24: std::cout << "SLA H"; break;
        case 0x25: std::cout << "SLA L"; break;
        case 0x27: std::cout << "SLA A"; break;
        case 0x26: std::cout << "SLA (HL)"; break;
        case 0x30: std::cout << "SWAP B"; break;
        case 0x31: std::cout << "SWAP C"; break;
        case 0x32: std::cout << "SWAP D"; break;
        case 0x33: std::cout << "SWAP E"; break;
        case 0x34: std::cout << "SWAP H"; break;
        case 0x35: std::cout << "SWAP L"; break;
        case 0x37: std::cout << "SWAP A"; break;
        case 0x36: std::cout << "SWAP (HL)"; break;
        case 0x28: std::cout << "SRA B"; break;
        case 0x29: std::cout << "SRA C"; break;
        case 0x2A: std::cout << "SRA D"; break;
        case 0x2B: std::cout << "SRA E"; break;
        case 0x2C: std::cout << "SRA H"; break;
        case 0x2D: std::cout << "SRA L"; break;
        case 0x2F: std::cout << "SRA A"; break;
        case 0x2E: std::cout << "SRA (HL)"; break;
        case 0x38: std::cout << "SRL B"; break;
        case 0x39: std::cout << "SRL C"; break;
        case 0x3A: std::cout << "SRL D"; break;
        case 0x3B: std::cout << "SRL E"; break;
        case 0x3C: std::cout << "SRL H"; break;
        case 0x3D: std::cout << "SRL L"; break;
        case 0x3F: std::cout << "SRL A"; break;
        case 0x3E: std::cout << "SRL (HL)"; break;
            // 1-Bit Operations
        case 0x40: std::cout << "BIT 0,B"; break;
        case 0x41: std::cout << "BIT 0,C"; break;
        case 0x42: std::cout << "BIT 0,D"; break;
        case 0x43: std::cout << "BIT 0,E"; break;
        case 0x44: std::cout << "BIT 0,H"; break;
        case 0x45: std::cout << "BIT 0,L"; break;
        case 0x47: std::cout << "BIT 0,A"; break;
        case 0x48: std::cout << "BIT 1,B"; break;
        case 0x49: std::cout << "BIT 1,C"; break;
        case 0x4A: std::cout << "BIT 1,D"; break;
        case 0x4B: std::cout << "BIT 1,E"; break;
        case 0x4C: std::cout << "BIT 1,H"; break;
        case 0x4D: std::cout << "BIT 1,L"; break;
        case 0x4F: std::cout << "BIT 1,A"; break;
        case 0x50: std::cout << "BIT 2,B"; break;
        case 0x51: std::cout << "BIT 2,C"; break;
        case 0x52: std::cout << "BIT 2,D"; break;
        case 0x53: std::cout << "BIT 2,E"; break;
        case 0x54: std::cout << "BIT 2,H"; break;
        case 0x55: std::cout << "BIT 2,L"; break;
        case 0x57: std::cout << "BIT 2,A"; break;
        case 0x58: std::cout << "BIT 3,B"; break;
        case 0x59: std::cout << "BIT 3,C"; break;
        case 0x5A: std::cout << "BIT 3,D"; break;
        case 0x5B: std::cout << "BIT 3,E"; break;
        case 0x5C: std::cout << "BIT 3,H"; break;
        case 0x5D: std::cout << "BIT 3,L"; break;
        case 0x5F: std::cout << "BIT 3,A"; break;
        case 0x60: std::cout << "BIT 4,B"; break;
        case 0x61: std::cout << "BIT 4,C"; break;
        case 0x62: std::cout << "BIT 4,D"; break;
        case 0x63: std::cout << "BIT 4,E"; break;
        case 0x64: std::cout << "BIT 4,H"; break;
        case 0x65: std::cout << "BIT 4,L"; break;
        case 0x67: std::cout << "BIT 4,A"; break;
        case 0x68: std::cout << "BIT 5,B"; break;
        case 0x69: std::cout << "BIT 5,C"; break;
        case 0x6A: std::cout << "BIT 5,D"; break;
        case 0x6B: std::cout << "BIT 5,E"; break;
        case 0x6C: std::cout << "BIT 5,H"; break;
        case 0x6D: std::cout << "BIT 5,L"; break;
        case 0x6F: std::cout << "BIT 5,A"; break;
        case 0x70: std::cout << "BIT 6,B"; break;
        case 0x71: std::cout << "BIT 6,C"; break;
        case 0x72: std::cout << "BIT 6,D"; break;
        case 0x73: std::cout << "BIT 6,E"; break;
        case 0x74: std::cout << "BIT 6,H"; break;
        case 0x75: std::cout << "BIT 6,L"; break;
        case 0x77: std::cout << "BIT 6,A"; break;
        case 0x78: std::cout << "BIT 7,B"; break;
        case 0x79: std::cout << "BIT 7,C"; break;
        case 0x7A: std::cout << "BIT 7,D"; break;
        case 0x7B: std::cout << "BIT 7,E"; break;
        case 0x7C: std::cout << "BIT 7,H"; break;
        case 0x7D: std::cout << "BIT 7,L"; break;
        case 0x7F: std::cout << "BIT 7,A"; break;
        case 0x46: std::cout << "BIT 0,(HL)"; break;
        case 0x4E: std::cout << "BIT 1,(HL)"; break;
        case 0x56: std::cout << "BIT 2,(HL)"; break;
        case 0x5E: std::cout << "BIT 3,(HL)"; break;
        case 0x66: std::cout << "BIT 4,(HL)"; break;
        case 0x6E: std::cout << "BIT 5,(HL)"; break;
        case 0x76: std::cout << "BIT 6,(HL)"; break;
        case 0x7E: std::cout << "BIT 7,(HL)"; break;
        case 0xC0: std::cout << "SET 0,B"; break;
        case 0xC1: std::cout << "SET 0,C"; break;
        case 0xC2: std::cout << "SET 0,D"; break;
        case 0xC3: std::cout << "SET 0,E"; break;
        case 0xC4: std::cout << "SET 0,H"; break;
        case 0xC5: std::cout << "SET 0,L"; break;
        case 0xC7: std::cout << "SET 0,A"; break;
        case 0xC8: std::cout << "SET 1,B"; break;
        case 0xC9: std::cout << "SET 1,C"; break;
        case 0xCA: std::cout << "SET 1,D"; break;
        case 0xCB: std::cout << "SET 1,E"; break;
        case 0xCC: std::cout << "SET 1,H"; break;
        case 0xCD: std::cout << "SET 1,L"; break;
        case 0xCF: std::cout << "SET 1,A"; break;
        case 0xD0: std::cout << "SET 2,B"; break;
        case 0xD1: std::cout << "SET 2,C"; break;
        case 0xD2: std::cout << "SET 2,D"; break;
        case 0xD3: std::cout << "SET 2,E"; break;
        case 0xD4: std::cout << "SET 2,H"; break;
        case 0xD5: std::cout << "SET 2,L"; break;
        case 0xD7: std::cout << "SET 2,A"; break;
        case 0xD8: std::cout << "SET 3,B"; break;
        case 0xD9: std::cout << "SET 3,C"; break;
        case 0xDA: std::cout << "SET 3,D"; break;
        case 0xDB: std::cout << "SET 3,E"; break;
        case 0xDC: std::cout << "SET 3,H"; break;
        case 0xDD: std::cout << "SET 3,L"; break;
        case 0xDF: std::cout << "SET 3,A"; break;
        case 0xE0: std::cout << "SET 4,B"; break;
        case 0xE1: std::cout << "SET 4,C"; break;
        case 0xE2: std::cout << "SET 4,D"; break;
        case 0xE3: std::cout << "SET 4,E"; break;
        case 0xE4: std::cout << "SET 4,H"; break;
        case 0xE5: std::cout << "SET 4,L"; break;
        case 0xE7: std::cout << "SET 4,A"; break;
        case 0xE8: std::cout << "SET 5,B"; break;
        case 0xE9: std::cout << "SET 5,C"; break;
        case 0xEA: std::cout << "SET 5,D"; break;
        case 0xEB: std::cout << "SET 5,E"; break;
        case 0xEC: std::cout << "SET 5,H"; break;
        case 0xED: std::cout << "SET 5,L"; break;
        case 0xEF: std::cout << "SET 5,A"; break;
        case 0xF0: std::cout << "SET 6,B"; break;
        case 0xF1: std::cout << "SET 6,C"; break;
        case 0xF2: std::cout << "SET 6,D"; break;
        case 0xF3: std::cout << "SET 6,E"; break;
        case 0xF4: std::cout << "SET 6,H"; break;
        case 0xF5: std::cout << "SET 6,L"; break;
        case 0xF7: std::cout << "SET 6,A"; break;
        case 0xF8: std::cout << "SET 7,B"; break;
        case 0xF9: std::cout << "SET 7,C"; break;
        case 0xFA: std::cout << "SET 7,D"; break;
        case 0xFB: std::cout << "SET 7,E"; break;
        case 0xFC: std::cout << "SET 7,H"; break;
        case 0xFD: std::cout << "SET 7,L"; break;
        case 0xFF: std::cout << "SET 7,A"; break;
        case 0xC6: std::cout << "SET 0,(HL)"; break;
        case 0xCE: std::cout << "SET 1,(HL)"; break;
        case 0xD6: std::cout << "SET 2,(HL)"; break;
        case 0xDE: std::cout << "SET 3,(HL)"; break;
        case 0xE6: std::cout << "SET 4,(HL)"; break;
        case 0xEE: std::cout << "SET 5,(HL)"; break;
        case 0xF6: std::cout << "SET 6,(HL)"; break;
        case 0xFE: std::cout << "SET 7,(HL)"; break;
        case 0x80: std::cout << "RES 0,B"; break;
        case 0x81: std::cout << "RES 0,C"; break;
        case 0x82: std::cout << "RES 0,D"; break;
        case 0x83: std::cout << "RES 0,E"; break;
        case 0x84: std::cout << "RES 0,H"; break;
        case 0x85: std::cout << "RES 0,L"; break;
        case 0x87: std::cout << "RES 0,A"; break;
        case 0x88: std::cout << "RES 1,B"; break;
        case 0x89: std::cout << "RES 1,C"; break;
        case 0x8A: std::cout << "RES 1,D"; break;
        case 0x8B: std::cout << "RES 1,E"; break;
        case 0x8C: std::cout << "RES 1,H"; break;
        case 0x8D: std::cout << "RES 1,L"; break;
        case 0x8F: std::cout << "RES 1,A"; break;
        case 0x90: std::cout << "RES 2,B"; break;
        case 0x91: std::cout << "RES 2,C"; break;
        case 0x92: std::cout << "RES 2,D"; break;
        case 0x93: std::cout << "RES 2,E"; break;
        case 0x94: std::cout << "RES 2,H"; break;
        case 0x95: std::cout << "RES 2,L"; break;
        case 0x97: std::cout << "RES 2,A"; break;
        case 0x98: std::cout << "RES 3,B"; break;
        case 0x99: std::cout << "RES 3,C"; break;
        case 0x9A: std::cout << "RES 3,D"; break;
        case 0x9B: std::cout << "RES 3,E"; break;
        case 0x9C: std::cout << "RES 3,H"; break;
        case 0x9D: std::cout << "RES 3,L"; break;
        case 0x9F: std::cout << "RES 3,A"; break;
        case 0xA0: std::cout << "RES 4,B"; break;
        case 0xA1: std::cout << "RES 4,C"; break;
        case 0xA2: std::cout << "RES 4,D"; break;
        case 0xA3: std::cout << "RES 4,E"; break;
        case 0xA4: std::cout << "RES 4,H"; break;
        case 0xA5: std::cout << "RES 4,L"; break;
        case 0xA7: std::cout << "RES 4,A"; break;
        case 0xA8: std::cout << "RES 5,B"; break;
        case 0xA9: std::cout << "RES 5,C"; break;
        case 0xAA: std::cout << "RES 5,D"; break;
        case 0xAB: std::cout << "RES 5,E"; break;
        case 0xAC: std::cout << "RES 5,H"; break;
        case 0xAD: std::cout << "RES 5,L"; break;
        case 0xAF: std::cout << "RES 5,A"; break;
        case 0xB0: std::cout << "RES 6,B"; break;
        case 0xB1: std::cout << "RES 6,C"; break;
        case 0xB2: std::cout << "RES 6,D"; break;
        case 0xB3: std::cout << "RES 6,E"; break;
        case 0xB4: std::cout << "RES 6,H"; break;
        case 0xB5: std::cout << "RES 6,L"; break;
        case 0xB7: std::cout << "RES 6,A"; break;
        case 0xB8: std::cout << "RES 7,B"; break;
        case 0xB9: std::cout << "RES 7,C"; break;
        case 0xBA: std::cout << "RES 7,D"; break;
        case 0xBB: std::cout << "RES 7,E"; break;
        case 0xBC: std::cout << "RES 7,H"; break;
        case 0xBD: std::cout << "RES 7,L"; break;
        case 0xBF: std::cout << "RES 7,A"; break;
        case 0x86: std::cout << "RES 0,(HL)"; break;
        case 0x8E: std::cout << "RES 1,(HL)"; break;
        case 0x96: std::cout << "RES 2,(HL)"; break;
        case 0x9E: std::cout << "RES 3,(HL)"; break;
        case 0xA6: std::cout << "RES 4,(HL)"; break;
        case 0xAE: std::cout << "RES 5,(HL)"; break;
        case 0xB6: std::cout << "RES 6,(HL)"; break;
        case 0xBE: std::cout << "RES 7,(HL)"; break;
        default: assert(false);
    }
}

void disassembleOpcode(const BYTE& opcode){
    switch(opcode){
            // 8-Bit Loads
        case 0x78: std::cout << "LD A,B"; break;
        case 0x79: std::cout << "LD A,C"; break;
        case 0x7A: std::cout << "LD A,D"; break;
        case 0x7B: std::cout << "LD A,E"; break;
        case 0x7C: std::cout << "LD A,H"; break;
        case 0x7D: std::cout << "LD A,L"; break;
        case 0x7F: std::cout << "LD A,A"; break;
        case 0x40: std::cout << "LD B,B"; break;
        case 0x41: std::cout << "LD B,C"; break;
        case 0x42: std::cout << "LD B,D"; break;
        case 0x43: std::cout << "LD B,E"; break;
        case 0x44: std::cout << "LD B,H"; break;
        case 0x45: std::cout << "LD B,L"; break;
        case 0x47: std::cout << "LD B,A"; break;
        case 0x48: std::cout << "LD C,B"; break;
        case 0x49: std::cout << "LD C,C"; break;
        case 0x4A: std::cout << "LD C,D"; break;
        case 0x4B: std::cout << "LD C,E"; break;
        case 0x4C: std::cout << "LD C,H"; break;
        case 0x4D: std::cout << "LD C,L"; break;
        case 0x4F: std::cout << "LD C,A"; break;
        case 0x50: std::cout << "LD D,B"; break;
        case 0x51: std::cout << "LD D,C"; break;
        case 0x52: std::cout << "LD D,D"; break;
        case 0x53: std::cout << "LD D,E"; break;
        case 0x54: std::cout << "LD D,H"; break;
        case 0x55: std::cout << "LD D,L"; break;
        case 0x57: std::cout << "LD D,A"; break;
        case 0x58: std::cout << "LD E,B"; break;
        case 0x59: std::cout << "LD E,C"; break;
        case 0x5A: std::cout << "LD E,D"; break;
        case 0x5B: std::cout << "LD E,E"; break;
        case 0x5C: std::cout << "LD E,H"; break;
        case 0x5D: std::cout << "LD E,L"; break;
        case 0x5F: std::cout << "LD E,A"; break;
        case 0x60: std::cout << "LD H,B"; break;
        case 0x61: std::cout << "LD H,C"; break;
        case 0x62: std::cout << "LD H,D"; break;
        case 0x63: std::cout << "LD H,E"; break;
        case 0x64: std::cout << "LD H,H"; break;
        case 0x65: std::cout << "LD H,L"; break;
        case 0x67: std::cout << "LD H,A"; break;
        case 0x68: std::cout << "LD L,B"; break;
        case 0x69: std::cout << "LD L,C"; break;
        case 0x6A: std::cout << "LD L,D"; break;
        case 0x6B: std::cout << "LD L,E"; break;
        case 0x6C: std::cout << "LD L,H"; break;
        case 0x6D: std::cout << "LD L,L"; break;
        case 0x6F: std::cout << "LD L,A"; break;
        case 0x3E: std::cout << "LD A,$" << (int) mmu.readByte(PC); break;
        case 0x06: std::cout << "LD B,$" << (int) mmu.readByte(PC); break;
        case 0x0E: std::cout << "LD C,$" << (int) mmu.readByte(PC); break;
        case 0x16: std::cout << "LD D,$" << (int) mmu.readByte(PC); break;
        case 0x1E: std::cout << "LD E,$" << (int) mmu.readByte(PC); break;
        case 0x26: std::cout << "LD H,$" << (int) mmu.readByte(PC); break;
        case 0x2E: std::cout << "LD L,$" << (int) mmu.readByte(PC); break;
        case 0x7E: std::cout << "LD A,(HL)"; break;
        case 0x46: std::cout << "LD B,(HL)"; break;
        case 0x4E: std::cout << "LD C,(HL)"; break;
        case 0x56: std::cout << "LD D,(HL)"; break;
        case 0x5E: std::cout << "LD E,(HL)"; break;
        case 0x66: std::cout << "LD H,(HL)"; break;
        case 0x6E: std::cout << "LD L,(HL)"; break;
        case 0x70: std::cout << "LD (HL),B"; break;
        case 0x71: std::cout << "LD (HL),C"; break;
        case 0x72: std::cout << "LD (HL),D"; break;
        case 0x73: std::cout << "LD (HL),E"; break;
        case 0x74: std::cout << "LD (HL),H"; break;
        case 0x75: std::cout << "LD (HL),L"; break;
        case 0x77: std::cout << "LD (HL),B"; break;
        case 0x36: std::cout << "LD (HL),$" << (int) mmu.readByte(PC); break;
        case 0x0A: std::cout << "LD A,(BC)"; break;
        case 0x1A: std::cout << "LD A,(DE)"; break;
        case 0xFA: std::cout << "LD A,$" << (int) mmu.readByte(mmu.readWord(PC)); break;
        case 0x02: std::cout << "LD (BC),A"; break;
        case 0x12: std::cout << "LD (DE),A"; break;
        case 0xEA: std::cout << "LD ($" << (int) mmu.readWord(PC) << "),A"; break;
        case 0x08: std::cout << "LD ($" << (int) mmu.readWord(PC) << "),SP"; break;
        case 0xF0: std::cout << "LD A,$" << (int) (0xFF00 + mmu.readByte(PC)); break;
        case 0xE0: std::cout << "LD ($" << (int) (0xFF00 + mmu.readByte(PC)) << "),A"; break;
        case 0xF2: std::cout << "LD A,($ff00+C)"; break;
        case 0xE2: std::cout << "LD ($ff00+C),A"; break;
        case 0x22: std::cout << "LDI (HL),A"; break;
        case 0x2A: std::cout << "LD A,(HL+)"; break;
        case 0x32: std::cout << "LD (HL-),A"; break;
        case 0x3A: std::cout << "LD A,(HL-)"; break;
            // 16-Bit Loads
        case 0x01: std::cout << "LD BC,$" << (int) mmu.readWord(PC); break;
        case 0x11: std::cout << "LD DE,$" << (int) mmu.readWord(PC); break;
        case 0x21: std::cout << "LD HL,$" << (int) mmu.readWord(PC); break;
        case 0x31: std::cout << "LD SP,$" << (int) mmu.readWord(PC); break;
        case 0xF9: std::cout << "LD SP,HL"; break;
        case 0xC5: std::cout << "PUSH BC"; break;
        case 0xD5: std::cout << "PUSH DE"; break;
        case 0xE5: std::cout << "PUSH HL"; break;
        case 0xF5: std::cout << "PUSH AF"; break;
        case 0xC1: std::cout << "POP BC"; break;
        case 0xD1: std::cout << "POP DE"; break;
        case 0xE1: std::cout << "POP HL"; break;
        case 0xF1: std::cout << "POP AF"; break;
            // 8-Bit Arithmetic
        case 0x80: std::cout << "ADD A,B"; break;
        case 0x81: std::cout << "ADD A,C"; break;
        case 0x82: std::cout << "ADD A,D"; break;
        case 0x83: std::cout << "ADD A,E"; break;
        case 0x84: std::cout << "ADD A,H"; break;
        case 0x85: std::cout << "ADD A,L"; break;
        case 0x87: std::cout << "ADD A,A"; break;
        case 0xC6: std::cout << "ADD A,$" << (int) mmu.readByte(PC); break;
        case 0x86: std::cout << "ADD A,(HL)"; break;
        case 0x88: std::cout << "ADC A,B"; break;
        case 0x89: std::cout << "ADC A,C"; break;
        case 0x8A: std::cout << "ADC A,D"; break;
        case 0x8B: std::cout << "ADC A,E"; break;
        case 0x8C: std::cout << "ADC A,H"; break;
        case 0x8D: std::cout << "ADC A,L"; break;
        case 0x8F: std::cout << "ADC A,A"; break;
        case 0xCE: std::cout << "ADC A,$" << (int) mmu.readByte(PC); break;
        case 0x8E: std::cout << "ADC A,(HL)"; break;
        case 0x90: std::cout << "SUB B"; break;
        case 0x91: std::cout << "SUB C"; break;
        case 0x92: std::cout << "SUB D"; break;
        case 0x93: std::cout << "SUB E"; break;
        case 0x94: std::cout << "SUB H"; break;
        case 0x95: std::cout << "SUB L"; break;
        case 0x97: std::cout << "SUB A"; break;
        case 0xD6: std::cout << "SUB $" << (int) mmu.readByte(PC); break;
        case 0x96: std::cout << "SUB (HL)"; break;
        case 0x98: std::cout << "SBC B"; break;
        case 0x99: std::cout << "SBC C"; break;
        case 0x9A: std::cout << "SBC D"; break;
        case 0x9B: std::cout << "SBC E"; break;
        case 0x9C: std::cout << "SBC H"; break;
        case 0x9D: std::cout << "SBC L"; break;
        case 0x9F: std::cout << "SBC A"; break;
        case 0xDE: std::cout << "SBC $" << (int) mmu.readByte(PC); break;
        case 0x9E: std::cout << "SBC (HL)"; break;
        case 0xA0: std::cout << "AND B"; break;
        case 0xA1: std::cout << "AND C"; break;
        case 0xA2: std::cout << "AND D"; break;
        case 0xA3: std::cout << "AND E"; break;
        case 0xA4: std::cout << "AND H"; break;
        case 0xA5: std::cout << "AND L"; break;
        case 0xA7: std::cout << "AND A"; break;
        case 0xE6: std::cout << "AND $" << (int) mmu.readByte(PC); break;
        case 0xA6: std::cout << "AND (HL)"; break;
        case 0xA8: std::cout << "XOR B"; break;
        case 0xA9: std::cout << "XOR C"; break;
        case 0xAA: std::cout << "XOR D"; break;
        case 0xAB: std::cout << "XOR E"; break;
        case 0xAC: std::cout << "XOR H"; break;
        case 0xAD: std::cout << "XOR L"; break;
        case 0xAF: std::cout << "XOR A"; break;
        case 0xEE: std::cout << "XOR $" << (int) mmu.readByte(PC); break;
        case 0xAE: std::cout << "XOR (HL)"; break;
        case 0xB0: std::cout << "OR B"; break;
        case 0xB1: std::cout << "OR C"; break;
        case 0xB2: std::cout << "OR D"; break;
        case 0xB3: std::cout << "OR E"; break;
        case 0xB4: std::cout << "OR H"; break;
        case 0xB5: std::cout << "OR L"; break;
        case 0xB7: std::cout << "OR A"; break;
        case 0xF6: std::cout << "OR $" << (int) mmu.readByte(PC); break;
        case 0xB6: std::cout << "OR (HL)"; break;
        case 0xB8: std::cout << "CP B"; break;
        case 0xB9: std::cout << "CP C"; break;
        case 0xBA: std::cout << "CP D"; break;
        case 0xBB: std::cout << "CP E"; break;
        case 0xBC: std::cout << "CP H"; break;
        case 0xBD: std::cout << "CP L"; break;
        case 0xBF: std::cout << "CP A"; break;
        case 0xFE: std::cout << "CP $" << (int) mmu.readByte(PC); break;
        case 0xBE: std::cout << "CP (HL)"; break;
        case 0x04: std::cout << "INC B"; break;
        case 0x0C: std::cout << "INC C"; break;
        case 0x14: std::cout << "INC D"; break;
        case 0x1C: std::cout << "INC E"; break;
        case 0x24: std::cout << "INC H"; break;
        case 0x2C: std::cout << "INC L"; break;
        case 0x3C: std::cout << "INC A"; break;
        case 0x34: std::cout << "INC (HL)"; break;
        case 0x05: std::cout << "DEC B"; break;
        case 0x0D: std::cout << "DEC C"; break;
        case 0x15: std::cout << "DEC D"; break;
        case 0x1D: std::cout << "DEC E"; break;
        case 0x25: std::cout << "DEC H"; break;
        case 0x2D: std::cout << "DEC L"; break;
        case 0x3D: std::cout << "DEC A"; break;
        case 0x35: std::cout << "DEC (HL)"; break;
        case 0x27: std::cout << "DAA"; break;
        case 0x2F: std::cout << "CPL"; break;
            // 16-Bit Arithmetic/Logical Commands
        case 0x09: std::cout << "ADD HL,BC"; break;
        case 0x19: std::cout << "ADD HL,DE"; break;
        case 0x29: std::cout << "ADD HL,HL"; break;
        case 0x39: std::cout << "ADD HL,SP"; break;
        case 0x03: std::cout << "INC BC"; break;
        case 0x13: std::cout << "INC DE"; break;
        case 0x23: std::cout << "INC HL"; break;
        case 0x33: std::cout << "INC SP"; break;
        case 0x0B: std::cout << "DEC BC"; break;
        case 0x1B: std::cout << "DEC DE"; break;
        case 0x2B: std::cout << "DEC HL"; break;
        case 0x3B: std::cout << "DEC SP"; break;
        case 0xE8: std::cout << "ADD SP,$" << (int) mmu.readByte(PC); break;
        case 0xF8: std::cout << "LD HL,SP+$" << (int) mmu.readByte(PC); break;
            // Rotate and Shift Commands
        case 0x07: std::cout << "RLCA"; break;
        case 0x17: std::cout << "RLA"; break;
        case 0x0F: std::cout << "RRCA"; break;
        case 0x1F: std::cout << "RRA"; break;
            // Includes the rotate/shift + 1-bit operations
        case 0xCB: return disassembleExtendedOpcode(mmu.readByte(PC));
            // CPU-Control Commands
        case 0x3F: std::cout << "CCF"; break;
        case 0x37: std::cout << "SCF"; break;
        case 0x00: std::cout << "NOP"; break;
        case 0x76: std::cout << "HALT"; break;
        case 0x10: std::cout << "STOP 0"; break;
        case 0xF3: std::cout << "DI"; break;
        case 0xFB: std::cout << "EI"; break;
            // Jump Commands
        case 0xC3: std::cout << "JP $" << (int) mmu.readWord(PC); break;
        case 0xE9: std::cout << "JP (HL)"; break;
        case 0xC2: std::cout << "JP NZ,$" << (int) mmu.readWord(PC); break;
        case 0xCA: std::cout << "JP Z,$" << (int) mmu.readWord(PC); break;
        case 0xD2: std::cout << "JP NC,$" << (int) mmu.readWord(PC); break;
        case 0xDA: std::cout << "JP C,$" << (int) mmu.readWord(PC); break;
        case 0x18: std::cout << "JR $" << (int) (PC + (SIGNED_BYTE) mmu.readByte(PC)); break;
        case 0x20: std::cout << "JR NZ,$" << (int) (PC + (SIGNED_BYTE) mmu.readByte(PC)); break;
        case 0x28: std::cout << "JR Z,$" << (int) (PC + (SIGNED_BYTE) mmu.readByte(PC)); break;
        case 0x30: std::cout << "JR NC,$" << (int) (PC + (SIGNED_BYTE) mmu.readByte(PC)); break;
        case 0x38: std::cout << "JR C,$" << (int) (PC + (SIGNED_BYTE) mmu.readByte(PC)); break;
        case 0xCD: std::cout << "CALL $" << (int) mmu.readWord(PC); break;
        case 0xC4: std::cout << "CALL NZ,$" << (int) mmu.readWord(PC); break;
        case 0xCC: std::cout << "CALL Z,$" << (int) mmu.readWord(PC); break;
        case 0xD4: std::cout << "CALL NC,$" << (int) mmu.readWord(PC); break;
        case 0xDC: std::cout << "CALL C,$" << (int) mmu.readWord(PC); break;
        case 0xC9: std::cout << "RET"; break;
        case 0xC0: std::cout << "RET NZ"; break;
        case 0xC8: std::cout << "RET Z"; break;
        case 0xD0: std::cout << "RET NC"; break;
        case 0xD8: std::cout << "RET C"; break;
        case 0xD9: std::cout << "RETI"; break;
        case 0xC7: std::cout << "RST $0000"; break;
        case 0xCF: std::cout << "RST $0008"; break;
        case 0xD7: std::cout << "RST $0010"; break;
        case 0xDF: std::cout << "RST $0018"; break;
        case 0xE7: std::cout << "RST $0020"; break;
        case 0xEF: std::cout << "RST $0028"; break;
        case 0xF7: std::cout << "RST $0030"; break;
        case 0xFF: std::cout << "RST $0038"; break;
        default: assert(false);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void printState(){
    std::cout << std::hex;
    std::cout << "PC: " << PC << std::endl;
    std::cout << "[";
    std::cout << "AF: $" << AF.reg << " |";
    std::cout << " BC: $" << BC.reg << " |";
    std::cout << " DE: $" << DE.reg << " |";
    std::cout << " HL: $" << HL.reg << " |";
    std::cout << " SP: $" << SP.reg;
    std::cout << "]" << std::endl;
}

void printLog(){
    disassembleOpcode(mmu.readByte(PC++));
    PC--;
    std::cout << std::endl;
    printState();
}

void printTileSet(){
    for(int i = 0; i < 384; i++){
        for(int y = 0; y < 8; y++){
            for(int x = 0; x < 8; x++){
                std::cout << (int) mmu.tileSet[i][y][x];
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
}

void printTileMap(){
    for(int y = 0; y < 32; y++){
        for(int x = 0; x < 32; x++){
            std::cout << (int) mmu.readByte(0x9800 + ((y * 32) + x));
        }
        std::cout << std::endl;
    }
}

int main(int argc, char *argv[]){
    
    const int maxCycles = (CLOCKSPEED / 60);
    int frameCycles = 0;
    int clockCycles = 0;
    
    mmu.reset();
    cpu.reset();
    ppu.reset();
    
    mmu.readROM(argv[1]);
    mmu.updateBanking();
    
    PC = 0xFE;
    SP.reg = 0xFFFE;
    HL.reg = 0x014D;
    BC.lo = 0x13;
    DE.hi = 0xD8;
    AF.hi = 0x01;
    
    SDL_Event e;
    bool quit = false;
    
    while (!quit){
        while (SDL_PollEvent(&e)){
            switch(e.type){
                case SDL_QUIT:
                    quit = true; break;
                case SDL_KEYDOWN:
                    joypad.keyDown(e.key.keysym.sym);
                    break;
                case SDL_KEYUP:
                    joypad.keyUp(e.key.keysym.sym);
                    break;
                default:
                    break;
            }
        }
        
        auto startTime = std::chrono::system_clock::now();
        while (frameCycles < maxCycles){
            //printState();
            //printLog();
//            if(PC == 0x358){
//                printTileSet();
//                std::cout << std::endl;
//                printTileMap();
//                std::exit(0);
//            }
            
            clockCycles = cpu.step();
            frameCycles += clockCycles;
            cpu.addToClock(clockCycles);
            ppu.addToClock(clockCycles);
            timer.addToClock(clockCycles);
            ppu.step();
            cpu.handleInterrupts();
        }

        frameCycles %= maxCycles;
        auto endTime = std::chrono::system_clock::now();
        
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        auto frameCap = std::chrono::milliseconds(200 / 60);
        
        if (diff < frameCap)
            std::this_thread::sleep_for(frameCap - diff);
        
    }
    ppu.quit();
}
