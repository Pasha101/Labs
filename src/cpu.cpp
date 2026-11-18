#include "cpu.hpp"
#include "memory.hpp"
#include "alu.hpp"

#include <iostream>
#include <iomanip>

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

    switch (opcode) {
        // 0x0_ — керування й вивід
        case 0x00: // HALT
            cpu.halted = true;
            cpu.pc += 1;
            break;
        case 0x01: // NOP
            cpu.pc += 1;
            break;
        case 0x02: // OUT (символ)
            std::cout << static_cast<char>(cpu.a);
            cpu.pc += 1;
            break;
        case 0x03: // OUTN (десяткове число і пробіл)
            std::cout << static_cast<int>(cpu.a) << ' ';
            cpu.pc += 1;
            break;

        // 0x1_ — ALU
        case 0x10: // ADD A, B
            cpu.a = alu_add(cpu.a, cpu.b, cpu.flags);
            cpu.pc += 1;
            break;
        case 0x11: // SUB A, B
            cpu.a = alu_sub(cpu.a, cpu.b, cpu.flags);
            cpu.pc += 1;
            break;
        case 0x12: // AND A, B
            cpu.a = alu_and(cpu.a, cpu.b, cpu.flags);
            cpu.pc += 1;
            break;
        case 0x13: // OR A, B
            cpu.a = alu_or(cpu.a, cpu.b, cpu.flags);
            cpu.pc += 1;
            break;
        case 0x14: // XOR A, B
            cpu.a = alu_xor(cpu.a, cpu.b, cpu.flags);
            cpu.pc += 1;
            break;
        case 0x15: // NOT A
            cpu.a = alu_not(cpu.a, cpu.flags);
            cpu.pc += 1;
            break;
        case 0x16: // SHL A
            cpu.a = alu_shl(cpu.a, cpu.flags);
            cpu.pc += 1;
            break;
        case 0x17: // SHR A
            cpu.a = alu_shr(cpu.a, cpu.flags);
            cpu.pc += 1;
            break;
        case 0x18: // INC A
            cpu.a = alu_inc(cpu.a, cpu.flags);
            cpu.pc += 1;
            break;
        case 0x19: // DEC A
            cpu.a = alu_dec(cpu.a, cpu.flags);
            cpu.pc += 1;
            break;
        case 0x1A: // CMP A, B (рахує A - B, ставить прапорці, результат викидає)
            (void)alu_sub(cpu.a, cpu.b, cpu.flags);
            cpu.pc += 1;
            break;

        // 0x2_ — пересилання даних
        case 0x20: // LOADI A, imm8
            cpu.a = mem_get(mem, cpu.pc + 1);
            cpu.pc += 2;
            break;
        case 0x21: // LOADI B, imm8
            cpu.b = mem_get(mem, cpu.pc + 1);
            cpu.pc += 2;
            break;
        case 0x22: { // LOAD A, [addr16]
            std::uint16_t addr = get16(mem, cpu.pc + 1);
            cpu.a = mem_get(mem, addr);
            cpu.pc += 3;
            break;
        }
        case 0x23: { // LOAD B, [addr16]
            std::uint16_t addr = get16(mem, cpu.pc + 1);
            cpu.b = mem_get(mem, addr);
            cpu.pc += 3;
            break;
        }
        case 0x24: { // STORE [addr16], A
            std::uint16_t addr = get16(mem, cpu.pc + 1);
            mem_set(mem, addr, cpu.a);
            cpu.pc += 3;
            break;
        }
        case 0x25: { // STORE [addr16], B
            std::uint16_t addr = get16(mem, cpu.pc + 1);
            mem_set(mem, addr, cpu.b);
            cpu.pc += 3;
            break;
        }
        case 0x26: // MOV A, B
            cpu.a = cpu.b;
            cpu.pc += 1;
            break;
        case 0x27: // MOV B, A
            cpu.b = cpu.a;
            cpu.pc += 1;
            break;
        case 0x28: // LOADH H, imm16
            cpu.h = get16(mem, cpu.pc + 1);
            cpu.pc += 3;
            break;
        case 0x29: // LOAD A, [H]
            cpu.a = mem_get(mem, cpu.h);
            cpu.pc += 1;
            break;
        case 0x2A: // STORE [H], A
            mem_set(mem, cpu.h, cpu.a);
            cpu.pc += 1;
            break;
        case 0x2B: // INCH
            cpu.h = static_cast<std::uint16_t>(cpu.h + 1);
            cpu.pc += 1;
            break;
        case 0x2C: // DECH
            cpu.h = static_cast<std::uint16_t>(cpu.h - 1);
            cpu.pc += 1;
            break;
        case 0x2D: // HLOW (A = H & 0xFF)
            cpu.a = static_cast<Byte>(cpu.h & 0xFF);
            cpu.pc += 1;
            break;

        // 0x3_ — стрибки (Lab 04)
        case 0x30: // JMP addr16
            cpu.pc = get16(mem, cpu.pc + 1);
            break;
        case 0x31: // JZ addr16
            if (cpu.flags.z) {
                cpu.pc = get16(mem, cpu.pc + 1);
            } else {
                cpu.pc += 3;
            }
            break;
        case 0x32: // JNZ addr16
            if (!cpu.flags.z) {
                cpu.pc = get16(mem, cpu.pc + 1);
            } else {
                cpu.pc += 3;
            }
            break;
        case 0x33: // JC addr16 (opt)
            if (cpu.flags.c) {
                cpu.pc = get16(mem, cpu.pc + 1);
            } else {
                cpu.pc += 3;
            }
            break;
        case 0x34: // JNC addr16 (opt)
            if (!cpu.flags.c) {
                cpu.pc = get16(mem, cpu.pc + 1);
            } else {
                cpu.pc += 3;
            }
            break;

        default:
            std::cout << "unknown opcode 0x" << std::hex << std::setfill('0') << std::setw(2)
                      << static_cast<int>(opcode) << std::dec << "\n";
            cpu.halted = true;
            return false;
    }

    return true;
}