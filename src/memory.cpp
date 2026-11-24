#include "memory.hpp"

Byte mem_get(const Memory& mem, std::size_t addr) {
    if (addr >= MEM_SIZE) {
        return 0;
    }
    return mem.data[addr];
}

bool mem_set(Memory& mem, std::size_t addr, Byte value) {
    if (addr >= MEM_SIZE) {
        return false;
    }
    mem.data[addr] = value;
    return true;
}


std::uint16_t get16(const Memory& mem, std::size_t addr) {
    if (addr + 1 >= MEM_SIZE) {
        return 0;
    }
    Byte low = mem_get(mem, addr);
    Byte high = mem_get(mem, addr + 1);
    return static_cast<std::uint16_t>(low) | (static_cast<std::uint16_t>(high) << 8);
}


bool set16(Memory& mem, std::size_t addr, std::uint16_t value) {
    if (addr + 1 >= MEM_SIZE) {
        return false;
    }
    Byte low = static_cast<Byte>(value & 0xFF);
    Byte high = static_cast<Byte>((value >> 8) & 0xFF);
    mem_set(mem, addr, low);
    mem_set(mem, addr + 1, high);
    return true;
}