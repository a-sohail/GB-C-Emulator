#ifndef registers_hpp
#define registers_hpp

#include "definitions.hpp"

union Register{
    WORD reg;
    struct{
        BYTE lo;
        BYTE hi;
    };
};

extern Register AF;
extern Register BC;
extern Register DE;
extern Register HL;
extern Register SP;
extern WORD PC;
extern WORD ifRegister;
extern WORD ieRegister;

#endif /* registers_hpp */
