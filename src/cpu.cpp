#include "cpu.hpp"
#include "memory.hpp"
#include "alu.hpp"
#include "screen.hpp"
#include "stack.hpp"

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
              << " | SP: 0x" << std::setw(4) << cpu.stack.sp
              << " | HEAP: 0x" << std::setw(4) << cpu.heap_ptr
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

    Byte raw_op = mem_get(mem, cpu.pc);
    Op op = static_cast<Op>(raw_op);
    cpu.ticks++;

    switch (op) {
        case Op::Halt:
            cpu.halted = true;
            cpu.pc += 1;
            break;
        case Op::Nop:
            cpu.pc += 1;
            break;
        case Op::Out:
            std::cout << static_cast<char>(cpu.a);
            cpu.pc += 1;
            break;
        case Op::Outn:
            std::cout << static_cast<int>(cpu.a) << ' ';
            cpu.pc += 1;
            break;
        case Op::Outs: {
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

        case Op::Add:
            cpu.a = alu_add(cpu.a, cpu.b, cpu.flags);
            cpu.pc += 1;
            break;
        case Op::Sub:
            cpu.a = alu_sub(cpu.a, cpu.b, cpu.flags);
            cpu.pc += 1;
            break;
        case Op::And:
            cpu.a = alu_and(cpu.a, cpu.b, cpu.flags);
            cpu.pc += 1;
            break;
        case Op::Or:
            cpu.a = alu_or(cpu.a, cpu.b, cpu.flags);
            cpu.pc += 1;
            break;
        case Op::Xor:
            cpu.a = alu_xor(cpu.a, cpu.b, cpu.flags);
            cpu.pc += 1;
            break;
        case Op::Not:
            cpu.a = alu_not(cpu.a, cpu.flags);
            cpu.pc += 1;
            break;
        case Op::Shl:
            cpu.a = alu_shl(cpu.a, cpu.flags);
            cpu.pc += 1;
            break;
        case Op::Shr:
            cpu.a = alu_shr(cpu.a, cpu.flags);
            cpu.pc += 1;
            break;
        case Op::Inc:
            cpu.a = alu_inc(cpu.a, cpu.flags);
            cpu.pc += 1;
            break;
        case Op::Dec:
            cpu.a = alu_dec(cpu.a, cpu.flags);
            cpu.pc += 1;
            break;
        case Op::Cmp:
            (void)alu_sub(cpu.a, cpu.b, cpu.flags);
            cpu.pc += 1;
            break;
        case Op::Mul:
            cpu.a = alu_mul(cpu.a, cpu.b, cpu.flags);
            cpu.pc += 1;
            break;

        case Op::LoadiA:
            cpu.a = mem_get(mem, cpu.pc + 1);
            cpu.pc += 2;
            break;
        case Op::LoadiB:
            cpu.b = mem_get(mem, cpu.pc + 1);
            cpu.pc += 2;
            break;
        case Op::LoadA: {
            std::uint16_t addr = get16(mem, cpu.pc + 1);
            cpu.a = guest_read(cpu, mem, addr);
            cpu.pc += 3;
            break;
        }
        case Op::LoadB: {
            std::uint16_t addr = get16(mem, cpu.pc + 1);
            cpu.b = guest_read(cpu, mem, addr);
            cpu.pc += 3;
            break;
        }
        case Op::StoreA: {
            std::uint16_t addr = get16(mem, cpu.pc + 1);
            guest_write(mem, addr, cpu.a);
            cpu.pc += 3;
            break;
        }
        case Op::StoreB: {
            std::uint16_t addr = get16(mem, cpu.pc + 1);
            guest_write(mem, addr, cpu.b);
            cpu.pc += 3;
            break;
        }
        case Op::MovAB:
            cpu.a = cpu.b;
            cpu.pc += 1;
            break;
        case Op::MovBA:
            cpu.b = cpu.a;
            cpu.pc += 1;
            break;
        case Op::LoadH:
            cpu.h = get16(mem, cpu.pc + 1);
            cpu.pc += 3;
            break;
        case Op::LoadAH:
            cpu.a = guest_read(cpu, mem, cpu.h);
            cpu.pc += 1;
            break;
        case Op::StoreHA:
            guest_write(mem, cpu.h, cpu.a);
            cpu.pc += 1;
            break;
        case Op::IncH:
            cpu.h = static_cast<std::uint16_t>(cpu.h + 1);
            cpu.pc += 1;
            break;
        case Op::DecH:
            cpu.h = static_cast<std::uint16_t>(cpu.h - 1);
            cpu.pc += 1;
            break;
        case Op::HLow:
            cpu.a = static_cast<Byte>(cpu.h & 0xFF);
            cpu.pc += 1;
            break;

        case Op::Jmp:
            cpu.pc = get16(mem, cpu.pc + 1);
            break;
        case Op::Jz:
            if (cpu.flags.z) {
                cpu.pc = get16(mem, cpu.pc + 1);
            } else {
                cpu.pc += 3;
            }
            break;
        case Op::Jnz:
            if (!cpu.flags.z) {
                cpu.pc = get16(mem, cpu.pc + 1);
            } else {
                cpu.pc += 3;
            }
            break;
        case Op::Jc:
            if (cpu.flags.c) {
                cpu.pc = get16(mem, cpu.pc + 1);
            } else {
                cpu.pc += 3;
            }
            break;
        case Op::Jnc:
            if (!cpu.flags.c) {
                cpu.pc = get16(mem, cpu.pc + 1);
            } else {
                cpu.pc += 3;
            }
            break;

        case Op::Cls:
            clear(mem);
            cpu.pc += 1;
            break;
        case Op::Plot:
            cpu.flags.c = !plot(mem, static_cast<int>(cpu.a), static_cast<int>(cpu.b));
            cpu.pc += 1;
            break;
        case Op::Unplot:
            cpu.flags.c = !unplot(mem, static_cast<int>(cpu.a), static_cast<int>(cpu.b));
            cpu.pc += 1;
            break;
        case Op::Show:
            show(mem);
            cpu.pc += 1;
            break;

        case Op::PushA:
            if (!stack_push(cpu.stack, mem, cpu.a)) {
                std::cout << "\nstack overflow at PC=0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.pc << std::dec << "\n";
                cpu.halted = true;
                return false;
            }
            cpu.pc += 1;
            break;
        case Op::PushB:
            if (!stack_push(cpu.stack, mem, cpu.b)) {
                std::cout << "\nstack overflow at PC=0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.pc << std::dec << "\n";
                cpu.halted = true;
                return false;
            }
            cpu.pc += 1;
            break;
        case Op::PopA:
            if (!stack_pop(cpu.stack, mem, cpu.a)) {
                std::cout << "\nstack underflow at PC=0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.pc << std::dec << "\n";
                cpu.halted = true;
                return false;
            }
            cpu.pc += 1;
            break;
        case Op::PopB:
            if (!stack_pop(cpu.stack, mem, cpu.b)) {
                std::cout << "\nstack underflow at PC=0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.pc << std::dec << "\n";
                cpu.halted = true;
                return false;
            }
            cpu.pc += 1;
            break;
        case Op::Call: {
            std::uint16_t target = get16(mem, cpu.pc + 1);
            std::uint16_t ret_addr = static_cast<std::uint16_t>(cpu.pc + 3);
            Byte lo = static_cast<Byte>(ret_addr & 0xFF);
            Byte hi = static_cast<Byte>((ret_addr >> 8) & 0xFF);
            if (!stack_push(cpu.stack, mem, lo) || !stack_push(cpu.stack, mem, hi)) {
                std::cout << "\nstack overflow on CALL at PC=0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.pc << std::dec << "\n";
                cpu.halted = true;
                return false;
            }
            cpu.pc = target;
            break;
        }
        case Op::Ret: {
            Byte hi = 0;
            Byte lo = 0;
            if (!stack_pop(cpu.stack, mem, hi) || !stack_pop(cpu.stack, mem, lo)) {
                std::cout << "\nstack underflow on RET at PC=0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.pc << std::dec << "\n";
                cpu.halted = true;
                return false;
            }
            cpu.pc = (static_cast<std::uint16_t>(hi) << 8) | static_cast<std::uint16_t>(lo);
            break;
        }
        case Op::PushH: {
            Byte lo = static_cast<Byte>(cpu.h & 0xFF);
            Byte hi = static_cast<Byte>((cpu.h >> 8) & 0xFF);
            if (!stack_push(cpu.stack, mem, lo) || !stack_push(cpu.stack, mem, hi)) {
                std::cout << "\nstack overflow at PC=0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.pc << std::dec << "\n";
                cpu.halted = true;
                return false;
            }
            cpu.pc += 1;
            break;
        }
        case Op::PopH: {
            Byte hi = 0;
            Byte lo = 0;
            if (!stack_pop(cpu.stack, mem, hi) || !stack_pop(cpu.stack, mem, lo)) {
                std::cout << "\nstack underflow at PC=0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.pc << std::dec << "\n";
                cpu.halted = true;
                return false;
            }
            cpu.h = (static_cast<std::uint16_t>(hi) << 8) | static_cast<std::uint16_t>(lo);
            cpu.pc += 1;
            break;
        }

        case Op::Alloc: {
            std::uint32_t size = static_cast<std::uint32_t>(cpu.a);
            std::uint32_t limit = static_cast<std::uint32_t>(HEAP_HI) + 1u;
            if (static_cast<std::uint32_t>(cpu.heap_ptr) + size <= limit) {
                cpu.h = cpu.heap_ptr;
                cpu.heap_ptr = static_cast<std::uint16_t>(cpu.heap_ptr + size);
                cpu.flags.c = false;
            } else {
                cpu.flags.c = true;
            }
            cpu.pc += 1;
            break;
        }

        default:
            cpu.ticks--;
            std::cout << "unknown opcode 0x" << std::hex << std::setfill('0') << std::setw(2)
                      << static_cast<int>(raw_op) << std::dec << "\n";
            cpu.halted = true;
            return false;
    }

    return true;
}