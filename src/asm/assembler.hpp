#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include "../memory.hpp"

struct AsmResult {
    bool ok{false};
    std::size_t bytes_written{0};
    std::string error;
};

AsmResult assemble_source(const std::string& src, Memory& mem, std::uint16_t base_addr = 0x0000);
void disassemble_range(const Memory& mem, std::uint16_t lo, std::uint16_t hi);