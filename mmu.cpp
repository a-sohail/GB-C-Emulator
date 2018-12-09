#include "mmu.hpp"

void MMU::updateTileSet(WORD addr){
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

void MMU::updateSpriteSet(WORD addr, BYTE val){
    int spriteNumber = (addr - 0xFE00) >> 2;
    
    SPRITE& sprite = spriteSet[spriteNumber];
    
    switch((addr - 0xFE00) & 0x03){
        case 0:
            sprite.posY = val - 16;
            break;
        case 1:
            sprite.posX = val - 8;
            break;
        case 2:
            sprite.tileNumber = val;
            break;
        case 3:
            sprite.prioritized = (val & 0x80) ? 0 : 1;
            sprite.flippedY = (val & 0x40) ? 1 : 0;
            sprite.flippedX = (val & 0x20) ? 1 : 0;
            sprite.zeroPalette = (val & 0x10) ? 0 : 1;
            break;
    }
}

void MMU::setPalette(BYTE palette[4][4], BYTE val){
    for(int i = 0; i < 4; i++){
        switch((val >> (i * 2)) & 3){
            case 0: palette[i][0] = 255; palette[i][1] = 255; palette[i][2] = 255; palette[i][3] = 255; break;
            case 1: palette[i][0] = 192; palette[i][1] = 192; palette[i][2] = 192; palette[i][3] = 255; break;
            case 2: palette[i][0] = 96; palette[i][1] = 96; palette[i][2] = 96; palette[i][3] = 255; break;
            case 3: palette[i][0] = 0; palette[i][1] = 0; palette[i][2] = 0; palette[i][3] = 255; break;
        }
    }
}

void MMU::dmaTransfer(const BYTE& val) {
    WORD startAddr = val * 0x100;
    
    for (BYTE i = 0x0; i <= 0x9F; i++) {
        WORD fromAddr = startAddr + i;
        WORD toAddr = 0xFE00 + i;
        
        BYTE valAtAddr = readByte(fromAddr);
        writeByte(toAddr, valAtAddr);
    }
}


void MMU::reset(){
    memset(&memory, 0, GAMEBOY_MEMORY);
    memset(&cartridgeMemory, 0, MAX_MEMORY);
    memset(&ramMemory, 0, sizeof(ramMemory));
}

void MMU::updateBanking(){
    switch(cartridgeMemory[0x147]){
        case 1  : mbc1 = true; break;
        case 2  : mbc1 = true; break;
        case 3  : mbc1 = true; break;
        default : mbc1 = false; break;
    }
}

void MMU::handleBanking(WORD address, BYTE val){
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
            romBankNumber = ((val & 0x3) << 5) + (romBankNumber & 0x1F);
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

BYTE MMU::readByte(WORD address){
    
    if(inBIOS && address < 0x100){
        return bootROM[address];
    }
    else if(address < 0x4000){
        return cartridgeMemory[address];
    }
    else if (address >= 0x4000 && address <= 0x7FFF){
        return cartridgeMemory[(address - 0x4000) + (romBankNumber * 0x4000)];
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
    else if((address >= 0xFF10 && address <= 0xFF3F) &&
            !(address >= 0xFF27 && address <= 0xFF2F)){
        return apu.readByte(address);
    }
    else if(address == 0xFF40){
        return (switchBG      ? 0x01 : 0x00) |
        (switchOBJ     ? 0x02 : 0x00) |
        (spriteDoubled ? 0x04 : 0x00) |
        (bgMap         ? 0x08 : 0x00) |
        (bgTile        ? 0x10 : 0x00) |
        (switchWindow  ? 0x20 : 0x00) |
        (windowTile    ? 0x40 : 0x00) |
        (switchLCD     ? 0x80 : 0x00);
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
    else if (address == 0xFF4A){
        return windowY;
    }
    else if (address == 0xFF4B){
        return windowX;
    }
    else if(address == 0xFF0F){
        return ifRegister;
    }
    else if(address == 0xFFFF){
        return ieRegister;
    }
    
    return memory[address];
}

void MMU::writeByte(WORD address, BYTE val){
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
    else if((address >= 0xFF10 && address <= 0xFF3F) &&
            !(address >= 0xFF27 && address <= 0xFF2F)){
        apu.writeByte(address, val);
        return;
    }
    else if(address == 0xFF40){
        switchBG      = (val & 0x01) ? 1 : 0;
        switchOBJ     = (val & 0x02) ? 1 : 0;
        spriteDoubled = (val & 0x04) ? 1 : 0;
        bgMap         = (val & 0x08) ? 1 : 0;
        bgTile        = (val & 0x10) ? 1 : 0;
        switchWindow  = (val & 0x20) ? 1 : 0;
        windowTile    = (val & 0x40) ? 1 : 0;
        switchLCD     = (val & 0x80) ? 1 : 0;
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
    else if(address == 0xFF4A){
        windowY = val;
        return;
    }
    else if(address == 0xFF4B){
        windowX = val;
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

WORD MMU::readWord(WORD address){
    BYTE lowerByte = readByte(address);
    BYTE higherByte = readByte(address + 1);
    return lowerByte | (higherByte << 8);
}

void MMU::writeWord(WORD address, WORD val){
    writeByte(address, val);
    writeByte(address + 1, val >> 8);
}

// Read Rom
void MMU::readROM(const std::string& rom){
    FILE * file = fopen(rom.c_str(), "rb");
    if (file == NULL) return;
    fread(cartridgeMemory, 1, MAX_MEMORY, file);
    fclose(file);
    memcpy(memory, cartridgeMemory, GAMEBOY_MEMORY*sizeof(BYTE));
}

MMU mmu;
