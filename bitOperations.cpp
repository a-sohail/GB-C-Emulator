#include "bitOperations.hpp"
#include "registers.hpp"

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
