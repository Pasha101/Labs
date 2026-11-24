#pragma once

#include <cstddef>
#include <cstdint>
#include "alu.hpp"
#include "memory.hpp"

struct Cpu {
    std::size_t pc{0};
    Byte a{0};
    Byte b{0};
    std::uint16_t h{0};
    Flags flags{};
    bool halted{false};
    std::size_t ticks{0};
};

bool step(Cpu& cpu, Memory& mem);
void print_regs(const Cpu& cpu);