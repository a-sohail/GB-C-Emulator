#include "main.hpp"

int main(int argc, char *argv[]){
    
    const int maxCycles = (CLOCKSPEED / 60);
    int frameCycles = 0;
    int clockCycles = 0;
    
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    
    mmu.reset();
    cpu.reset();
    ppu.reset();
    apu.reset();
    
    mmu.readROM(argv[1]);
    
    // Check if MBC1 or not. Other types not supported (yet).
    mmu.updateBanking();
    
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
        
        // Remnant of controlling CPU pacing, sound now implicitly controls this. May change in the future.
        //auto startTime = std::chrono::system_clock::now();
        
        while (frameCycles < maxCycles){
            clockCycles = cpu.step();
            frameCycles += clockCycles;
            cpu.addToClock(clockCycles);
            ppu.addToClock(clockCycles);
            timer.addToClock(clockCycles);
            ppu.step();
            apu.step(clockCycles);
            cpu.handleInterrupts();
        }
        
        frameCycles %= maxCycles;
        
        // Remnant of controlling CPU pacing, sound now implicitly controls this. May change in the future.
        //        auto endTime = std::chrono::system_clock::now();
        //
        //        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        //        auto frameCap = std::chrono::milliseconds(200 / 60);
        //
        //        if (diff < frameCap){
        //            std::this_thread::sleep_for(frameCap - diff);
        //        }
        
    }
    ppu.quit();
}
