#ifndef debug_hpp
#define debug_hpp

#include "definitions.hpp"

class Debug{
    
public:
    
    void disassembleExtendedOpcode(const BYTE& opcode);
    void disassembleOpcode(const BYTE& opcode);
    void printState();
    void printLog();
    void printTileSet();
    void printTileMap();
};

extern Debug debug;

#endif /* debug_hpp */
