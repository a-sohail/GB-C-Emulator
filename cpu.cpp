#include "cpu.hpp"
#include "mmu.hpp"
#include "bitOperations.hpp"

// 8-bit loads
void CPU::CPU_LOAD(BYTE& b1, const BYTE& b2){
    b1 = b2;
}

void CPU::CPU_LOAD_WRITE(const WORD& w, const BYTE& b){
    mmu.writeByte(w, b);
}

// 16-bit loads
void CPU::CPU_LOAD_16BIT(WORD& reg, const WORD& val, const SIGNED_BYTE& immValue, bool isStackPointer){
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

void CPU::CPU_LOAD_WRITE_16BIT(const WORD& w, const WORD& b){
    mmu.writeWord(w, b);
}

void CPU::CPU_PUSH(const WORD& reg){
    SP.reg -= 2;
    mmu.writeWord(SP.reg, reg);
}

void CPU::CPU_POP(WORD& reg){
    reg = mmu.readWord(SP.reg);
    SP.reg += 2;
}

// 8-bit Arithmetic/Logical Commands
void CPU::CPU_ADD(const BYTE& b, bool carry){
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

void CPU::CPU_SUB(const BYTE& b, bool carry){
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

void CPU::CPU_AND(const BYTE& b){
    AF.hi &= b;
    AF.lo = 0;
    if(AF.hi == 0){
        flagBit(AF.lo, FLAG_Z);
    }
    flagBit(AF.lo, FLAG_HC);
}

void CPU::CPU_XOR(const BYTE& b){
    AF.hi ^= b;
    AF.lo = 0;
    if(AF.hi == 0){
        flagBit(AF.lo, FLAG_Z);
    }
}

void CPU::CPU_OR(const BYTE& b){
    AF.hi |= b;
    AF.lo = 0;
    if(AF.hi == 0){
        flagBit(AF.lo, FLAG_Z);
    }
}

void CPU::CPU_CP(const BYTE& b){
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

void CPU::CPU_INC(BYTE& b){
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

void CPU::CPU_INC_WRITE(){
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

void CPU::CPU_DEC(BYTE& b){
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

void CPU::CPU_DEC_WRITE(){
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

void CPU::CPU_DAA(){
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

void CPU::CPU_CPL(){
    AF.hi = ~AF.hi;
    flagBit(AF.lo, FLAG_N);
    flagBit(AF.lo, FLAG_HC);
}

// 16-bit Arithmetic/Logical Commands
void CPU::CPU_ADD_16BIT(WORD& reg, const WORD& val){
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

void CPU::CPU_ADD_16BIT_SIGNED(WORD& reg, const SIGNED_BYTE& val){
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

void CPU::CPU_INC_16BIT(WORD& reg){
    reg++;
}

void CPU::CPU_DEC_16BIT(WORD& reg){
    reg--;
}

void CPU::CPU_RLC(BYTE& reg){
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

void CPU::CPU_RLCA(BYTE& reg){
    bool msb = isFlagged(reg, 0x80);
    reg <<= 1;
    AF.lo = 0;
    if(msb){
        flagBit(reg, 0x01);
        flagBit(AF.lo, FLAG_C);
    }
}

void CPU::CPU_RL(BYTE& reg){
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

void CPU::CPU_RLA(BYTE& reg){
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

void CPU::CPU_RRC(BYTE& reg){
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

void CPU::CPU_RR(BYTE& reg){
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

void CPU::CPU_RRCA(BYTE& reg){
    bool lsb = isFlagged(reg, 0x01);
    reg >>= 1;
    AF.lo = 0;
    if(lsb){
        flagBit(reg, 0x80);
        flagBit(AF.lo, FLAG_C);
    }
}

void CPU::CPU_RRA(BYTE& reg){
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

void CPU::CPU_RLC_WRITE(){
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

void CPU::CPU_RL_WRITE(){
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

void CPU::CPU_RRC_WRITE(){
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

void CPU::CPU_RR_WRITE(){
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

void CPU::CPU_SLA(BYTE& reg){
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

void CPU::CPU_SLA_WRITE(){
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

void CPU::CPU_SWAP(BYTE& reg){
    reg = (reg >> 4) | (reg << 4);
    AF.lo = 0;
    if(reg == 0){
        flagBit(AF.lo, FLAG_Z);
    }
}

void CPU::CPU_SWAP_WRITE(){
    BYTE reg = mmu.readByte(HL.reg);
    reg = (reg >> 4) | (reg << 4);
    mmu.writeByte(HL.reg, reg);
    AF.lo = 0;
    if(reg == 0){
        flagBit(AF.lo, FLAG_Z);
    }
}

void CPU::CPU_SRA(BYTE& reg){
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

void CPU::CPU_SRA_WRITE(){
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

void CPU::CPU_SRL(BYTE& reg){
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

void CPU::CPU_SRL_WRITE(){
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
void CPU::CPU_BIT(const BYTE& bit, const BYTE& reg){
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

void CPU::CPU_SET(const BYTE& bit, BYTE& reg){
    WORD mask = 256;
    reg = ((mask >> (8 - bit)) & 0xFF) | reg;
}

void CPU::CPU_SET_WRITE(const BYTE& bit){
    BYTE reg = mmu.readByte(HL.reg);
    WORD mask = 256;
    reg = ((mask >> (8 - bit)) & 0xFF) | reg;
    mmu.writeByte(HL.reg, reg);
}

void CPU::CPU_RES(const BYTE& bit, BYTE& reg){
    WORD mask = 256;
    reg = ~((mask >> (8 - bit)) & 0xFF) & reg;
}

void CPU::CPU_RES_WRITE(const BYTE& bit){
    BYTE reg = mmu.readByte(HL.reg);
    WORD mask = 256;
    reg = ~((mask >> (8 - bit)) & 0xFF) & reg;
    mmu.writeByte(HL.reg, reg);
}

// CPU Control
void CPU::CPU_CCF(){
    unflagBit(AF.lo, FLAG_N);
    unflagBit(AF.lo, FLAG_HC);
    if(isFlagged(AF.lo, FLAG_C)){
        unflagBit(AF.lo, FLAG_C);
    }
    else{
        flagBit(AF.lo, FLAG_C);
    }
}

void CPU::CPU_SCF(){
    unflagBit(AF.lo, FLAG_N);
    unflagBit(AF.lo, FLAG_HC);
    flagBit(AF.lo, FLAG_C);
}

// NOP ignored because we'll handle clock cycle updates in the switch statement

void CPU::CPU_HALT(){
    if(!IME && (ifRegister & ieRegister & 0x1F)){
        halt = false;
        return;
    }
    PC--;
    halt = true;
}

void CPU::CPU_DI(){
    IME = false;
}

void CPU::CPU_EI(){
    IME = true;
}

// Jump Commands
void CPU::CPU_JP(bool useFlag, const WORD& address, const BYTE& flag, bool set){
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

void CPU::CPU_JR(bool useFlag, const SIGNED_BYTE& address, const BYTE& flag, bool set){
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

void CPU::CPU_CALL(bool useFlag, const WORD& address, const BYTE& flag, bool set){
    if(!useFlag || (useFlag && set && isFlagged(AF.lo, flag)) || (useFlag && !set && !isFlagged(AF.lo, flag)) ){
        CPU_PUSH(PC);
        PC = address;
    }
}

void CPU::CPU_RET(bool useFlag, const BYTE& flag, bool set){
    if(!useFlag || (useFlag && set && isFlagged(AF.lo, flag)) || (useFlag && !set && !isFlagged(AF.lo, flag))){
        CPU_POP(PC);
    }
}

void CPU::CPU_RETI(){
    CPU_RET(0, 0, 0);
    IME = true;
}

void CPU::CPU_RST(const WORD& address){
    CPU_CALL(false, address, 0, 0);
}

void CPU::CPU_RESET(){
    AF.reg = 0;
    BC.reg = 0;
    DE.reg = 0;
    HL.reg = 0;
    SP.reg = 0;
    PC = 0;
    clock = 0;
    ifRegister = 0x0;
}

int CPU::executeExtendedOpcode(const BYTE& opcode){
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

int CPU::executeOpcode(const BYTE& opcode){
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
        case 0x76: CPU_HALT(); if(!halt){return 4 + executeOpcode(mmu.readByte(PC));} return 4;
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

void CPU::reset(){
    CPU_RESET();
}

void CPU::addToClock(int clockCycles){
    clock += clockCycles;
}

void CPU::handleInterrupts(){
    
    // Check what interrupts are enabled by using the IE and IF registers respectively
    BYTE interrupts =  ifRegister & ieRegister & 0x1F;
    
    if(halt && interrupts){
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

int CPU::step(){
    return executeOpcode(mmu.readByte(PC++));
}

CPU cpu;
