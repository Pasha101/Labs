#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>
#include "dump.hpp"
#include "memory.hpp"
#include "cpu.hpp"

static bool parse_number(const std::string& word, long& out) {
    try {
        std::size_t used = 0;
        out = std::stol(word, &used, 0);
        return used == word.size();
    } catch (...) {
        return false;
    }
}

// Лінійний пошук першого i в [lo, hi), де mem_get(mem, i) == needle; якщо промах — повертає hi
static std::uint16_t find_byte(const Memory& mem, std::uint16_t lo, std::uint16_t hi, Byte needle) {
    for (std::uint16_t i = lo; i < hi; ++i) {
        if (mem_get(mem, i) == needle) {
            return i;
        }
    }
    return hi;
}

static void print_help() {
    std::cout << "commands:\n"
              << "  dump                   print all " << MEM_SIZE << " bytes\n"
              << "  get <addr>             show one byte four ways\n"
              << "  set <addr> <val>       write one byte (dec or 0x hex)\n"
              << "  set16 <addr> <v>       write 16-bit word (little-endian)\n"
              << "  find <lo> <hi> <byte>  linear search in [lo, hi)\n"
              << "  regs                   print cpu registers (PC A B H Z N C)\n"
              << "  step                   execute one instruction\n"
              << "  run                    execute instructions until HALT (limit 100000)\n"
              << "  b <addr>               run until PC == addr\n"
              << "  reg <a|b|h|pc> <v>     set register value\n"
              << "  help                   this list\n"
              << "  quit                   leave\n";
}

int main() {
    Memory mem;
    Cpu cpu;
    const std::size_t MAX_STEPS = 100000;

    std::cout << "ember 0.4 - The Shape of Control (Lab 04). Type `help`.\n";

    std::string line;
    while (true) {
        std::cout << "ember> ";

        if (!std::getline(std::cin, line)) {
            std::cout << '\n';
            break;
        }

        std::istringstream words(line);
        std::string cmd;
        words >> cmd;

        if (cmd.empty()) {
            continue;
        } else if (cmd == "quit" || cmd == "exit") {
            break;
        } else if (cmd == "help") {
            print_help();
        } else if (cmd == "dump") {
            dump(mem);
        } else if (cmd == "regs") {
            print_regs(cpu);
        } else if (cmd == "step") {
            step(cpu, mem);
        } else if (cmd == "run") {
            std::size_t step_count = 0;
            for (;;) {
                if (cpu.halted) {
                    break;
                }
                if (step_count >= MAX_STEPS) {
                    std::cout << "\nstep limit (" << MAX_STEPS << ") reached\n";
                    break;
                }
                if (!step(cpu, mem)) {
                    break;
                }
                step_count++;
            }
        } else if (cmd == "b") {
            std::string a;
            long target = 0;
            if (!(words >> a) || !parse_number(a, target) || target < 0 || target >= static_cast<long>(MEM_SIZE)) {
                std::cout << "usage: b <addr>\n";
            } else {
                std::size_t step_count = 0;
                std::size_t bp = static_cast<std::size_t>(target);
                while (!cpu.halted && cpu.pc != bp && step_count < MAX_STEPS) {
                    if (!step(cpu, mem)) {
                        break;
                    }
                    step_count++;
                }
                if (step_count >= MAX_STEPS) {
                    std::cout << "\nstep limit (" << MAX_STEPS << ") reached\n";
                }
            }
        } else if (cmd == "find") {
            std::string s_lo, s_hi, s_val;
            long lo = 0, hi = 0, val = 0;
            if (!(words >> s_lo) || !(words >> s_hi) || !(words >> s_val) ||
                !parse_number(s_lo, lo) || !parse_number(s_hi, hi) || !parse_number(s_val, val)) {
                std::cout << "usage: find <lo> <hi> <byte>\n";
            } else if (lo < 0 || hi < 0 || lo > hi || hi > static_cast<long>(MEM_SIZE)) {
                std::cout << "invalid range [lo, hi)\n";
            } else if (val < 0 || val > 255) {
                std::cout << "a byte is 0..255, got " << val << '\n';
            } else {
                std::uint16_t res = find_byte(mem,
                                              static_cast<std::uint16_t>(lo),
                                              static_cast<std::uint16_t>(hi),
                                              static_cast<Byte>(val));
                if (res == static_cast<std::uint16_t>(hi)) {
                    std::cout << "not found\n";
                } else {
                    std::cout << "found at 0x" << std::hex << std::setfill('0') << std::setw(4) << res
                              << " (" << std::dec << res << ")\n";
                }
            }
        } else if (cmd == "reg") {
            std::string reg_name, v;
            long val = 0;
            if (!(words >> reg_name) || !(words >> v) || !parse_number(v, val)) {
                std::cout << "usage: reg <a|b|h|pc> <val>\n";
            } else {
                if (reg_name == "a" || reg_name == "A") {
                    cpu.a = static_cast<Byte>(val & 0xFF);
                } else if (reg_name == "b" || reg_name == "B") {
                    cpu.b = static_cast<Byte>(val & 0xFF);
                } else if (reg_name == "h" || reg_name == "H") {
                    cpu.h = static_cast<std::uint16_t>(val & 0xFFFF);
                } else if (reg_name == "pc" || reg_name == "PC") {
                    cpu.pc = static_cast<std::size_t>(val & 0xFFFF);
                } else {
                    std::cout << "unknown register: " << reg_name << "\n";
                }
            }
        } else if (cmd == "get") {
            std::string a;
            long addr = 0;
            if (!(words >> a) || !parse_number(a, addr)) {
                std::cout << "usage: get <addr>\n";
            } else if (addr < 0) {
                std::cout << "address must not be negative\n";
            } else {
                show_byte(mem_get(mem, static_cast<std::size_t>(addr)));
            }
        } else if (cmd == "set") {
            std::string a, v;
            long addr = 0, value = 0;
            if (!(words >> a) || !(words >> v) || !parse_number(a, addr) || !parse_number(v, value)) {
                std::cout << "usage: set <addr> <value>\n";
            } else if (addr < 0) {
                std::cout << "address must not be negative\n";
            } else if (value < 0 || value > 255) {
                std::cout << "a byte is 0..255, got " << value << '\n';
            } else if (!mem_set(mem, static_cast<std::size_t>(addr), static_cast<Byte>(value))) {
                std::cout << "address " << addr << " is outside 0.." << MEM_SIZE - 1 << '\n';
            }
        } else if (cmd == "set16") {
            std::string a, v;
            long addr = 0, value = 0;
            if (!(words >> a) || !(words >> v) || !parse_number(a, addr) || !parse_number(v, value)) {
                std::cout << "usage: set16 <addr> <value>\n";
            } else if (addr < 0) {
                std::cout << "address must not be negative\n";
            } else if (!set16(mem, static_cast<std::size_t>(addr), static_cast<std::uint16_t>(value))) {
                std::cout << "address " << addr << " is outside bounds\n";
            }
        } else {
            std::cout << "unknown command: " << cmd << " (try `help`)\n";
        }
    }

    return 0;
}