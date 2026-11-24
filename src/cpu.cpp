#include "cpu.hpp"
#include "memory.hpp"
#include "alu.hpp"
#include "screen.hpp"

#include <iostream>
#include <iomanip>

static constexpr std::uint16_t PORT_OUT_ADDR = 0x0B00;
static constexpr std::uint16_t TICKS_ADDR    = 0x0B01;

static Byte guest_read(const Cpu& cpu, const Memory& mem, std::uint16_t addr) {
    if (addr == TICKS_ADDR) {
        return static_cast<Byte>(cpu.ticks & 0xFF);
    }
    return mem_get(mem, addr);
}

static void guest_write(Memory& mem, std::uint16_t addr, Byte val) {
    mem_set(mem, addr, val);
    if (addr == PORT_OUT_ADDR) {
        std::cout << static_cast<char>(val);
    }
}

void print_regs(const Cpu& cpu) {
    std::cout << "PC: " << std::hex << std::setfill('0') << std::setw(4) << cpu.pc
              << " | A: 0x" << std::setw(2) << static_cast<int>(cpu.a)
              << " | B: 0x" << std::setw(2) << static_cast<int>(cpu.b)
              << " | H: 0x" << std::setw(4) << cpu.h
              << std::dec
              << " | Z: " << cpu.flags.z
              << " N: " << cpu.flags.n
              << " C: " << cpu.flags.c
              << " | HALT: " << cpu.halted << "\n";
}

bool step(Cpu& cpu, Memory& mem) {
    if (cpu.halted) {
        std::cout << "CPU is halted.\n";
        return false;
    }

    if (cpu.pc >= MEM_SIZE) {
        std::cout << "PC out of bounds.\n";
        cpu.halted = true;
        return false;
    }

    Byte opcode = mem_get(mem, cpu.pc);
    cpu.ticks++;

    switch (opcode) {
        
        case 0x00: 
            cpu.halted = true;
            cpu.pc += 1;
            break;
        case 0x01: 
            cpu.pc += 1;
            break;
        case 0x02: 
            std::cout << static_cast<char>(cpu.a);
            cpu.pc += 1;
            break;
        case 0x03: 
            std::cout << static_cast<int>(cpu.a) << ' ';
            cpu.pc += 1;
            break;
        case 0x04: { 
            std::uint16_t addr = get16(mem, cpu.pc + 1);
            for (std::size_t i = 0; i < 256; ++i) {
                std::size_t cur = static_cast<std::size_t>(addr) + i;
                if (cur >= MEM_SIZE) {
                    break;
                }
                Byte ch = mem_get(mem, cur);
                if (ch == 0) {
                    break;
                }
                std::cout << static_cast<char>(ch);
            }
            cpu.pc += 3;
            break;
        }

     
        case 0x10: 
            cpu.a = alu_add(cpu.a, cpu.b, cpu.flags);
            cpu.pc += 1;
            break;
        case 0x11: 
            cpu.a = alu_sub(cpu.a, cpu.b, cpu.flags);
            cpu.pc += 1;
            break;
        case 0x12: 
            cpu.a = alu_and(cpu.a, cpu.b, cpu.flags);
            cpu.pc += 1;
            break;
        case 0x13: 
            cpu.a = alu_or(cpu.a, cpu.b, cpu.flags);
            cpu.pc += 1;
            break;
        case 0x14: 
            cpu.a = alu_xor(cpu.a, cpu.b, cpu.flags);
            cpu.pc += 1;
            break;
        case 0x15: 
            cpu.a = alu_not(cpu.a, cpu.flags);
            cpu.pc += 1;
            break;
        case 0x16: 
            cpu.a = alu_shl(cpu.a, cpu.flags);
            cpu.pc += 1;
            break;
        case 0x17: 
            cpu.a = alu_shr(cpu.a, cpu.flags);
            cpu.pc += 1;
            break;
        case 0x18: 
            cpu.a = alu_inc(cpu.a, cpu.flags);
            cpu.pc += 1;
            break;
        case 0x19: 
            cpu.a = alu_dec(cpu.a, cpu.flags);
            cpu.pc += 1;
            break;
        case 0x1A: 
            (void)alu_sub(cpu.a, cpu.b, cpu.flags);
            cpu.pc += 1;
            break;

      
        case 0x20: 
            cpu.a = mem_get(mem, cpu.pc + 1);
            cpu.pc += 2;
            break;
        case 0x21: 
            cpu.b = mem_get(mem, cpu.pc + 1);
            cpu.pc += 2;
            break;
        case 0x22: { 
            std::uint16_t addr = get16(mem, cpu.pc + 1);
            cpu.a = guest_read(cpu, mem, addr);
            cpu.pc += 3;
            break;
        }
        case 0x23: { 
            std::uint16_t addr = get16(mem, cpu.pc + 1);
            cpu.b = guest_read(cpu, mem, addr);
            cpu.pc += 3;
            break;
        }
        case 0x24: { 
            std::uint16_t addr = get16(mem, cpu.pc + 1);
            guest_write(mem, addr, cpu.a);
            cpu.pc += 3;
            break;
        }
        case 0x25: { 
            std::uint16_t addr = get16(mem, cpu.pc + 1);
            guest_write(mem, addr, cpu.b);
            cpu.pc += 3;
            break;
        }
        case 0x26: 
            cpu.a = cpu.b;
            cpu.pc += 1;
            break;
        case 0x27: 
            cpu.b = cpu.a;
            cpu.pc += 1;
            break;
        case 0x28:
            cpu.h = get16(mem, cpu.pc + 1);
            cpu.pc += 3;
            break;
        case 0x29: 
            cpu.a = guest_read(cpu, mem, cpu.h);
            cpu.pc += 1;
            break;
        case 0x2A: 
            guest_write(mem, cpu.h, cpu.a);
            cpu.pc += 1;
            break;
        case 0x2B: 
            cpu.h = static_cast<std::uint16_t>(cpu.h + 1);
            cpu.pc += 1;
            break;
        case 0x2C: 
            cpu.h = static_cast<std::uint16_t>(cpu.h - 1);
            cpu.pc += 1;
            break;
        case 0x2D: 
            cpu.a = static_cast<Byte>(cpu.h & 0xFF);
            cpu.pc += 1;
            break;

        
        case 0x30: 
            cpu.pc = get16(mem, cpu.pc + 1);
            break;
        case 0x31: 
            if (cpu.flags.z) {
                cpu.pc = get16(mem, cpu.pc + 1);
            } else {
                cpu.pc += 3;
            }
            break;
        case 0x32: 
            if (!cpu.flags.z) {
                cpu.pc = get16(mem, cpu.pc + 1);
            } else {
                cpu.pc += 3;
            }
            break;
        case 0x33:
            if (cpu.flags.c) {
                cpu.pc = get16(mem, cpu.pc + 1);
            } else {
                cpu.pc += 3;
            }
            break;
        case 0x34:
            if (!cpu.flags.c) {
                cpu.pc = get16(mem, cpu.pc + 1);
            } else {
                cpu.pc += 3;
            }
            break;

        
        case 0x40: 
            clear(mem);
            cpu.pc += 1;
            break;
        case 0x41: 
            cpu.flags.c = !plot(mem, static_cast<int>(cpu.a), static_cast<int>(cpu.b));
            cpu.pc += 1;
            break;
        case 0x42: 
            cpu.flags.c = !unplot(mem, static_cast<int>(cpu.a), static_cast<int>(cpu.b));
            cpu.pc += 1;
            break;
        case 0x43: 
            show(mem);
            cpu.pc += 1;
            break;

        default:
            cpu.ticks--;
            std::cout << "unknown opcode 0x" << std::hex << std::setfill('0') << std::setw(2)
                      << static_cast<int>(opcode) << std::dec << "\n";
            cpu.halted = true;
            return false;
    }

    return true;
}