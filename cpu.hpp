#ifndef cpu_hpp
#define cpu_hpp

// register.h
#define FLAG_Z 0x80
#define FLAG_N 0x40
#define FLAG_HC 0x20
#define FLAG_C 0x10

#include "definitions.hpp"

class CPU{
    
    int clock = 0;
    bool halt = false;
    bool IME = false;
    
    // 8-bit loads
    void CPU_LOAD(BYTE& b1, const BYTE& b2);
    void CPU_LOAD_WRITE(const WORD& w, const BYTE& b);
    
    // 16-bit loads
    void CPU_LOAD_16BIT(WORD& reg, const WORD& val, const SIGNED_BYTE& immValue, bool isStackPointer);
    void CPU_LOAD_WRITE_16BIT(const WORD& w, const WORD& b);
    void CPU_PUSH(const WORD& reg);
    void CPU_POP(WORD& reg);
    
    // 8-bit Arithmetic/Logical Commands
    void CPU_ADD(const BYTE& b, bool carry);
    void CPU_SUB(const BYTE& b, bool carry);
    void CPU_AND(const BYTE& b);
    void CPU_XOR(const BYTE& b);
    void CPU_OR(const BYTE& b);
    void CPU_CP(const BYTE& b);
    void CPU_INC(BYTE& b);
    void CPU_INC_WRITE();
    void CPU_DEC(BYTE& b);
    void CPU_DEC_WRITE();
    void CPU_DAA();
    void CPU_CPL();
    
    // 16-bit Arithmetic/Logical Commands
    void CPU_ADD_16BIT(WORD& reg, const WORD& val);
    void CPU_ADD_16BIT_SIGNED(WORD& reg, const SIGNED_BYTE& val);
    void CPU_INC_16BIT(WORD& reg);
    void CPU_DEC_16BIT(WORD& reg);
    void CPU_RLC(BYTE& reg);
    void CPU_RLCA(BYTE& reg);
    void CPU_RL(BYTE& reg);
    void CPU_RLA(BYTE& reg);
    void CPU_RRC(BYTE& reg);
    void CPU_RR(BYTE& reg);
    void CPU_RRCA(BYTE& reg);
    void CPU_RRA(BYTE& reg);
    void CPU_RLC_WRITE();
    void CPU_RL_WRITE();
    void CPU_RRC_WRITE();
    void CPU_RR_WRITE();
    void CPU_SLA(BYTE& reg);
    void CPU_SLA_WRITE();
    void CPU_SWAP(BYTE& reg);
    void CPU_SWAP_WRITE();
    void CPU_SRA(BYTE& reg);
    void CPU_SRA_WRITE();
    void CPU_SRL(BYTE& reg);
    void CPU_SRL_WRITE();
    
    // 1-bit Operations
    void CPU_BIT(const BYTE& bit, const BYTE& reg);
    void CPU_SET(const BYTE& bit, BYTE& reg);
    void CPU_SET_WRITE(const BYTE& bit);
    void CPU_RES(const BYTE& bit, BYTE& reg);
    void CPU_RES_WRITE(const BYTE& bit);
    
    // CPU Control
    void CPU_CCF();
    void CPU_SCF();
    
    // NOP ignored because we'll handle clock cycle updates in the switch statement
    void CPU_HALT();
    void CPU_DI();
    void CPU_EI();
    
    // Jump Commands
    void CPU_JP(bool useFlag, const WORD& address, const BYTE& flag, bool set);
    void CPU_JR(bool useFlag, const SIGNED_BYTE& address, const BYTE& flag, bool set);
    void CPU_CALL(bool useFlag, const WORD& address, const BYTE& flag, bool set);
    void CPU_RET(bool useFlag, const BYTE& flag, bool set);
    void CPU_RETI();
    void CPU_RST(const WORD& address);
    void CPU_RESET();
    
    int executeExtendedOpcode(const BYTE& opcode);
    int executeOpcode(const BYTE& opcode);
    
public:
    
    void reset();
    void addToClock(int clockCycles);
    void handleInterrupts();
    int step();
};

extern CPU cpu;

#endif /* cpu_hpp */
