#pragma once

#include <cstddef>
#include "memory.hpp"

const std::size_t VRAM_LO = 0x0A00;
const std::size_t VRAM_HI = 0x0AFF;
const int SCREEN_W = 64;
const int SCREEN_H = 32;

void show(const Memory& mem);
bool plot(Memory& mem, int x, int y);
bool unplot(Memory& mem, int x, int y);
void clear(Memory& mem);
void draw_frame(Memory& mem);