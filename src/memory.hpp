#pragma once

#include <cstddef>
#include <cstdint>

using Byte = std::uint8_t;
const std::size_t MEM_SIZE = 4096;

struct Memory {
    Byte data[MEM_SIZE]{};
};

Byte mem_get(const Memory& mem, std::size_t addr);
bool mem_set(Memory& mem, std::size_t addr, Byte value);

// Lab 03 additions: 16-bit access (Little-Endian)
std::uint16_t get16(const Memory& mem, std::size_t addr);
bool set16(Memory& mem, std::size_t addr, std::uint16_t value);