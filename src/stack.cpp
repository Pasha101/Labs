#include "stack.hpp"
#include <iostream>
#include <iomanip>

bool stack_empty(const Stack& s) {
    return s.sp >= STACK_HI;
}

bool stack_full(const Stack& s) {
    return s.sp < STACK_LO;
}

bool stack_push(Stack& s, Memory& mem, Byte val) {
    if (stack_full(s)) {
        return false;
    }
    if (!mem_set(mem, s.sp, val)) {
        return false;
    }
    s.sp = static_cast<std::uint16_t>(s.sp - 1);
    return true;
}

bool stack_pop(Stack& s, const Memory& mem, Byte& out) {
    if (stack_empty(s)) {
        return false;
    }
    s.sp = static_cast<std::uint16_t>(s.sp + 1);
    out = mem_get(mem, s.sp);
    return true;
}

void print_stack(const Stack& s, const Memory& mem) {
    std::size_t depth = static_cast<std::size_t>(STACK_HI - s.sp);
    std::cout << "SP: 0x" << std::hex << std::setfill('0') << std::setw(4) << s.sp
              << std::dec << " (depth: " << depth << " bytes)";
    if (depth == 0) {
        std::cout << " [empty]\n";
        return;
    }
    std::cout << " | top:";
    std::size_t shown = 0;
    for (std::uint16_t addr = static_cast<std::uint16_t>(s.sp + 1); addr <= STACK_HI && shown < 16; ++addr, ++shown) {
        std::cout << " 0x" << std::hex << std::setfill('0') << std::setw(2)
                  << static_cast<int>(mem_get(mem, addr));
    }
    std::cout << std::dec << "\n";
}