#include "screen.hpp"
#include <iostream>


void show(const Memory& mem) {
    for (int y = 0; y < SCREEN_H; ++y) {
        for (int x = 0; x < SCREEN_W; ++x) {
            std::size_t addr = VRAM_LO + static_cast<std::size_t>(y * 8 + (x / 8));
            Byte cell = mem_get(mem, addr);
            bool lit = ((cell >> (7 - (x % 8))) & 1u) != 0;
            std::cout << (lit ? '#' : '.');
        }
        std::cout << '\n';
    }
}


bool plot(Memory& mem, int x, int y) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) {
        return false;
    }
    std::size_t addr = VRAM_LO + static_cast<std::size_t>(y * 8 + (x / 8));
    int bit = 7 - (x % 8);
    Byte cell = mem_get(mem, addr);
    cell = static_cast<Byte>(cell | (1u << bit));
    return mem_set(mem, addr, cell);
}


bool unplot(Memory& mem, int x, int y) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) {
        return false;
    }
    std::size_t addr = VRAM_LO + static_cast<std::size_t>(y * 8 + (x / 8));
    int bit = 7 - (x % 8);
    Byte cell = mem_get(mem, addr);
    cell = static_cast<Byte>(cell & ~(1u << bit));
    return mem_set(mem, addr, cell);
}


void clear(Memory& mem) {
    for (std::size_t addr = VRAM_LO; addr <= VRAM_HI; ++addr) {
        mem_set(mem, addr, 0);
    }
}


void draw_frame(Memory& mem) {
    for (int x = 0; x < SCREEN_W; ++x) {
        plot(mem, x, 0);
        plot(mem, x, SCREEN_H - 1);
    }
    for (int y = 0; y < SCREEN_H; ++y) {
        plot(mem, 0, y);
        plot(mem, SCREEN_W - 1, y);
    }
}