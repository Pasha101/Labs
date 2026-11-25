#pragma once

#include <cstddef>
#include <cstdint>
#include "alu.hpp"
#include "memory.hpp"
#include "stack.hpp"

constexpr std::uint16_t HEAP_LO = 0x0C00;
constexpr std::uint16_t HEAP_HI = 0x0EFF;

enum class Op : Byte {
    Halt    = 0x00,
    Nop     = 0x01,
    Out     = 0x02,
    Outn    = 0x03,
    Outs    = 0x04,

    Add     = 0x10,
    Sub     = 0x11,
    And     = 0x12,
    Or      = 0x13,
    Xor     = 0x14,
    Not     = 0x15,
    Shl     = 0x16,
    Shr     = 0x17,
    Inc     = 0x18,
    Dec     = 0x19,
    Cmp     = 0x1A,
    Mul     = 0x1B,

    LoadiA  = 0x20,
    LoadiB  = 0x21,
    LoadA   = 0x22,
    LoadB   = 0x23,
    StoreA  = 0x24,
    StoreB  = 0x25,
    MovAB   = 0x26,
    MovBA   = 0x27,
    LoadH   = 0x28,
    LoadAH  = 0x29,
    StoreHA = 0x2A,
    IncH    = 0x2B,
    DecH    = 0x2C,
    HLow    = 0x2D,

    Jmp     = 0x30,
    Jz      = 0x31,
    Jnz     = 0x32,
    Jc      = 0x33,
    Jnc     = 0x34,

    Cls     = 0x40,
    Plot    = 0x41,
    Unplot  = 0x42,
    Show    = 0x43,

    PushA   = 0x50,
    PushB   = 0x51,
    PopA    = 0x52,
    PopB    = 0x53,
    Call    = 0x54,
    Ret     = 0x55,
    PushH   = 0x56,
    PopH    = 0x57,

    Alloc   = 0x60
};

struct Cpu {
    std::size_t pc{0};
    Byte a{0};
    Byte b{0};
    std::uint16_t h{0};
    Stack stack{};
    Flags flags{};
    bool halted{false};
    std::uint16_t heap_ptr{HEAP_LO};
    std::size_t ticks{0};
};

bool step(Cpu& cpu, Memory& mem);
void print_regs(const Cpu& cpu);