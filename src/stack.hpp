#pragma once

#include <cstdint>
#include "memory.hpp"

constexpr std::uint16_t STACK_LO = 0x0F00;
constexpr std::uint16_t STACK_HI = 0x0FFF;

struct Stack {
    std::uint16_t sp{STACK_HI};
};

bool stack_push(Stack& s, Memory& mem, Byte val);
bool stack_pop(Stack& s, const Memory& mem, Byte& out);
bool stack_empty(const Stack& s);
bool stack_full(const Stack& s);
void print_stack(const Stack& s, const Memory& mem);