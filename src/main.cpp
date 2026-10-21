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

static void print_help() {
    std::cout << "commands:\n"
              << "  dump              print all " << MEM_SIZE << " bytes\n"
              << "  get <addr>        show one byte four ways\n"
              << "  set <addr> <val>  write one byte (dec or 0x hex)\n"
              << "  set16 <addr> <v>  write 16-bit word (little-endian)\n"
              << "  regs              print cpu registers (PC A B H Z N C)\n"
              << "  step              execute one instruction\n"
              << "  run               execute instructions until HALT\n"
              << "  reg <a|b|h> <v>   set register value\n"
              << "  help              this list\n"
              << "  quit              leave\n";
}

int main() {
    Memory mem;
    Cpu cpu;

    std::cout << "ember 0.3 - Addresses, Pointers & Memory (Lab 03). Type `help`.\n";

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
            const std::size_t MAX_STEPS = 100000;
            while (!cpu.halted && step_count < MAX_STEPS) {
                step(cpu, mem);
                step_count++;
            }
            if (step_count >= MAX_STEPS) {
                std::cout << "Execution stopped: step limit exceeded.\n";
            }
        } else if (cmd == "reg") {
            std::string reg_name, v;
            long val = 0;
            if (!(words >> reg_name) || !(words >> v) || !parse_number(v, val)) {
                std::cout << "usage: reg <a|b|h> <val>\n";
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