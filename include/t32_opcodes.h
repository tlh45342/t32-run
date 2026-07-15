#ifndef T32_OPCODES_H
#define T32_OPCODES_H

#include <stdint.h>

typedef enum {
    T32_OPCODE_HALT  = 0x00,
    T32_OPCODE_NOP   = 0x01,
    T32_OPCODE_TRAP  = 0x02,
    T32_OPCODE_IRET  = 0x03,
    T32_OPCODE_CPUID = 0x04,

    T32_OPCODE_MOV   = 0x08,
    T32_OPCODE_MOVI  = 0x09,

    T32_OPCODE_LDB   = 0x10,
    T32_OPCODE_LDH   = 0x11,
    T32_OPCODE_LDW   = 0x12,
    T32_OPCODE_STB   = 0x13,
    T32_OPCODE_STH   = 0x14,
    T32_OPCODE_STW   = 0x15,

    T32_OPCODE_ADD   = 0x18,
    T32_OPCODE_ADDI  = 0x19,
    T32_OPCODE_SUB   = 0x1A,
    T32_OPCODE_SUBI  = 0x1B,
    T32_OPCODE_MUL   = 0x1C,
    T32_OPCODE_MULU  = 0x1D,
    T32_OPCODE_DIV   = 0x1E,
    T32_OPCODE_DIVU  = 0x1F,

    T32_OPCODE_AND   = 0x20,
    T32_OPCODE_OR    = 0x21,
    T32_OPCODE_XOR   = 0x22,
    T32_OPCODE_NOT   = 0x23,
    T32_OPCODE_SHL   = 0x24,
    T32_OPCODE_SHR   = 0x25,
    T32_OPCODE_SAR   = 0x26,

    T32_OPCODE_CMP   = 0x28,
    T32_OPCODE_CMPI  = 0x29,
    T32_OPCODE_JMP   = 0x2A,
    T32_OPCODE_JZ    = 0x2B,
    T32_OPCODE_JNZ   = 0x2C,

    T32_OPCODE_PUSH  = 0x30,
    T32_OPCODE_POP   = 0x31,
    T32_OPCODE_CALL  = 0x32,
    T32_OPCODE_RET   = 0x33
} t32_opcode_t;

const char *t32_opcode_name(uint8_t opcode);

#endif
