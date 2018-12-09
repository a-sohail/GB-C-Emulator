#ifndef bitOperations_hpp
#define bitOperations_hpp

#include "definitions.hpp"

extern void flagBit(BYTE& reg, const BYTE& b);
extern void unflagBit(BYTE& reg, const BYTE& b);
extern bool isFlagged(BYTE& reg, const BYTE& b);

#endif /* bitOperations_hpp */
