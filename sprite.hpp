#ifndef sprite_hpp
#define sprite_hpp

#include "definitions.hpp"

class SPRITE{
    
public:
    
    SIGNED_WORD posY;
    SIGNED_WORD posX;
    BYTE tileNumber;
    bool prioritized;
    bool flippedY;
    bool flippedX;
    bool zeroPalette;
};

#endif /* sprite_hpp */
