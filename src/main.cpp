#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>
#include <cstddef>
#include "dump.hpp"
#include "memory.hpp"
#include "cpu.hpp"
#include "screen.hpp"
#include "stack.hpp"

struct Sprite {
    std::uint8_t x{0};
    std::uint8_t y{0};
    std::int8_t  vx{0};
    std::int8_t  vy{0};
    bool alive{false};
};

constexpr std::size_t MAX_SPRITES = 8;

static bool parse_number(const std::string& word, long& out) {
    try {
        std::size_t used = 0;
        out = std::stol(word, &used, 0);
        return used == word.size();
    } catch (...) {
        return false;
    }
}

static std::uint16_t find_byte(const Memory& mem, std::uint16_t lo, std::uint16_t hi, Byte needle) {
    for (std::uint16_t i = lo; i < hi; ++i) {
        if (mem_get(mem, i) == needle) {
            return i;
        }
    }
    return hi;
}

static std::uint16_t find_nonzero(const Memory& mem, std::uint16_t lo, std::uint16_t hi) {
    for (std::uint16_t i = lo; i < hi; ++i) {
        if (mem_get(mem, i) != 0) {
            return i;
        }
    }
    return hi;
}

static void bubble_sort_mem(Memory& mem, std::uint16_t lo, std::uint16_t hi) {
    if (hi <= lo + 1) {
        return;
    }
    std::size_t n = static_cast<std::size_t>(hi - lo);
    for (std::size_t pass = 0; pass < n - 1; ++pass) {
        for (std::size_t i = 0; i < n - 1 - pass; ++i) {
            std::size_t a_addr = static_cast<std::size_t>(lo) + i;
            std::size_t b_addr = a_addr + 1;
            Byte va = mem_get(mem, a_addr);
            Byte vb = mem_get(mem, b_addr);
            if (va > vb) {
                mem_set(mem, a_addr, vb);
                mem_set(mem, b_addr, va);
            }
        }
    }
}

static void step_sprites(Sprite sprites[MAX_SPRITES], Memory& mem) {
    clear(mem);
    for (std::size_t i = 0; i < MAX_SPRITES; ++i) {
        Sprite& s = sprites[i];
        if (!s.alive) {
            continue;
        }
        int nx = static_cast<int>(s.x) + static_cast<int>(s.vx);
        int ny = static_cast<int>(s.y) + static_cast<int>(s.vy);

        if (nx < 0) {
            nx = -nx;
            s.vx = static_cast<std::int8_t>(-s.vx);
        } else if (nx >= SCREEN_W) {
            nx = 2 * (SCREEN_W - 1) - nx;
            s.vx = static_cast<std::int8_t>(-s.vx);
        }

        if (ny < 0) {
            ny = -ny;
            s.vy = static_cast<std::int8_t>(-s.vy);
        } else if (ny >= SCREEN_H) {
            ny = 2 * (SCREEN_H - 1) - ny;
            s.vy = static_cast<std::int8_t>(-s.vy);
        }

        s.x = static_cast<std::uint8_t>(nx);
        s.y = static_cast<std::uint8_t>(ny);
        plot(mem, s.x, s.y);
    }
}

static void print_layout() {
    std::cout << "=== Struct Memory Layout ===\n"
              << "sizeof(Flags)  = " << sizeof(Flags)  << " bytes (alignof = " << alignof(Flags)  << ")\n"
              << "  offsetof(z)  = " << offsetof(Flags, z) << "\n"
              << "  offsetof(n)  = " << offsetof(Flags, n) << "\n"
              << "  offsetof(c)  = " << offsetof(Flags, c) << "\n\n"
              << "sizeof(Stack)  = " << sizeof(Stack)  << " bytes (alignof = " << alignof(Stack)  << ")\n"
              << "  offsetof(sp) = " << offsetof(Stack, sp) << "\n\n"
              << "sizeof(Cpu)    = " << sizeof(Cpu)    << " bytes (alignof = " << alignof(Cpu)    << ")\n"
              << "  offsetof(pc)       = " << offsetof(Cpu, pc)       << " (8 bytes)\n"
              << "  offsetof(a)        = " << offsetof(Cpu, a)        << " (1 byte)\n"
              << "  offsetof(b)        = " << offsetof(Cpu, b)        << " (1 byte)\n"
              << "  offsetof(h)        = " << offsetof(Cpu, h)        << " (2 bytes)\n"
              << "  offsetof(stack)    = " << offsetof(Cpu, stack)    << " (2 bytes)\n"
              << "  offsetof(flags)    = " << offsetof(Cpu, flags)    << " (3 bytes)\n"
              << "  offsetof(halted)   = " << offsetof(Cpu, halted)   << " (1 byte)\n"
              << "  offsetof(heap_ptr) = " << offsetof(Cpu, heap_ptr) << " (2 bytes)\n"
              << "  [padding]          = 4 bytes (offsets 20..23 to align ticks to 8)\n"
              << "  offsetof(ticks)    = " << offsetof(Cpu, ticks)    << " (8 bytes)\n\n"
              << "sizeof(Sprite) = " << sizeof(Sprite) << " bytes (alignof = " << alignof(Sprite) << ")\n"
              << "  offsetof(x)     = " << offsetof(Sprite, x)     << "\n"
              << "  offsetof(y)     = " << offsetof(Sprite, y)     << "\n"
              << "  offsetof(vx)    = " << offsetof(Sprite, vx)    << "\n"
              << "  offsetof(vy)    = " << offsetof(Sprite, vy)    << "\n"
              << "  offsetof(alive) = " << offsetof(Sprite, alive) << "\n";
}

static void print_help() {
    std::cout << "commands:\n"
              << "  dump                         print all " << MEM_SIZE << " bytes\n"
              << "  get <addr>                   show one byte four ways\n"
              << "  set <addr> <val>             write one byte (dec or 0x hex)\n"
              << "  set16 <addr> <v>             write 16-bit word (little-endian)\n"
              << "  find <lo> <hi> <byte>        linear search in [lo, hi)\n"
              << "  findnz <lo> <hi>             find first non-zero byte in [lo, hi)\n"
              << "  sort <lo> <hi>               bubble sort bytes in [lo, hi)\n"
              << "  show                         render 64x32 VRAM (0x0A00..0x0AFF)\n"
              << "  cls                          clear 64x32 VRAM\n"
              << "  frame                        draw border frame in VRAM\n"
              << "  plot <x> <y>                 set pixel (x, y) in VRAM\n"
              << "  rowmin <y>                   leftmost lit pixel x in row y\n"
              << "  sprite <i> <x> <y> <vx> <vy> configure sprite 0..7\n"
              << "  tick [n]                     advance sprites n steps (default 1) and show\n"
              << "  layout                       print sizeof & field offsets of structs\n"
              << "  stack                        print SP and top stack bytes (0x0F00..0x0FFF)\n"
              << "  regs                         print cpu registers\n"
              << "  step                         execute one instruction\n"
              << "  run                          execute instructions until HALT (limit 100000)\n"
              << "  b <addr>                     run until PC == addr\n"
              << "  reg <a|b|h|sp|pc> <v>        set register value\n"
              << "  help                         this list\n"
              << "  quit                         leave\n";
}

int main() {
    Memory mem;
    Cpu cpu;
    Sprite sprites[MAX_SPRITES]{};
    const std::size_t MAX_STEPS = 100000;

    std::cout << "ember 0.7 - Call & Return: Functions, Stack & Recursion (Lab 07). Type `help`.\n";

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
        } else if (cmd == "layout") {
            print_layout();
        } else if (cmd == "stack") {
            print_stack(cpu.stack, mem);
        } else if (cmd == "dump") {
            dump(mem);
        } else if (cmd == "show") {
            show(mem);
        } else if (cmd == "cls") {
            clear(mem);
        } else if (cmd == "frame") {
            draw_frame(mem);
        } else if (cmd == "plot") {
            std::string sx, sy;
            long x = 0, y = 0;
            if (!(words >> sx) || !(words >> sy) || !parse_number(sx, x) || !parse_number(sy, y)) {
                std::cout << "usage: plot <x> <y>\n";
            } else if (!plot(mem, static_cast<int>(x), static_cast<int>(y))) {
                std::cout << "pixel out of bounds (0..63, 0..31)\n";
            }
        } else if (cmd == "sprite") {
            std::string si, sx, sy, svx, svy;
            long idx = 0, x = 0, y = 0, vx = 0, vy = 0;
            if (!(words >> si) || !(words >> sx) || !(words >> sy) || !(words >> svx) || !(words >> svy) ||
                !parse_number(si, idx) || !parse_number(sx, x) || !parse_number(sy, y) ||
                !parse_number(svx, vx) || !parse_number(svy, vy)) {
                std::cout << "usage: sprite <i> <x> <y> <vx> <vy>\n";
            } else if (idx < 0 || idx >= static_cast<long>(MAX_SPRITES)) {
                std::cout << "sprite index must be 0..7\n";
            } else if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) {
                std::cout << "sprite position out of bounds (0..63, 0..31)\n";
            } else if (vx < -32 || vx > 32 || vy < -16 || vy > 16) {
                std::cout << "velocity out of reasonable range\n";
            } else {
                sprites[idx] = Sprite{
                    static_cast<std::uint8_t>(x),
                    static_cast<std::uint8_t>(y),
                    static_cast<std::int8_t>(vx),
                    static_cast<std::int8_t>(vy),
                    true
                };
                plot(mem, static_cast<int>(x), static_cast<int>(y));
            }
        } else if (cmd == "tick") {
            std::string sn;
            long count = 1;
            if (words >> sn) {
                if (!parse_number(sn, count) || count < 1 || count > 1000) {
                    std::cout << "usage: tick [1..1000]\n";
                    continue;
                }
            }
            for (long step_i = 0; step_i < count; ++step_i) {
                step_sprites(sprites, mem);
            }
            show(mem);
        } else if (cmd == "rowmin") {
            std::string sy;
            long y = 0;
            if (!(words >> sy) || !parse_number(sy, y) || y < 0 || y >= SCREEN_H) {
                std::cout << "usage: rowmin <0..31>\n";
            } else {
                int found_x = -1;
                for (int x = 0; x < SCREEN_W; ++x) {
                    std::size_t addr = VRAM_LO + static_cast<std::size_t>(y * 8 + (x / 8));
                    Byte cell = mem_get(mem, addr);
                    if (((cell >> (7 - (x % 8))) & 1u) != 0) {
                        found_x = x;
                        break;
                    }
                }
                if (found_x < 0) {
                    std::cout << "none\n";
                } else {
                    std::cout << found_x << "\n";
                }
            }
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
        } else if (cmd == "findnz") {
            std::string s_lo, s_hi;
            long lo = 0, hi = 0;
            if (!(words >> s_lo) || !(words >> s_hi) ||
                !parse_number(s_lo, lo) || !parse_number(s_hi, hi)) {
                std::cout << "usage: findnz <lo> <hi>\n";
            } else if (lo < 0 || hi < 0 || lo > hi || hi > static_cast<long>(MEM_SIZE)) {
                std::cout << "invalid range [lo, hi)\n";
            } else {
                std::uint16_t res = find_nonzero(mem,
                                                 static_cast<std::uint16_t>(lo),
                                                 static_cast<std::uint16_t>(hi));
                if (res == static_cast<std::uint16_t>(hi)) {
                    std::cout << "not found\n";
                } else {
                    std::cout << "found at 0x" << std::hex << std::setfill('0') << std::setw(4) << res
                              << " (" << std::dec << res << ")\n";
                }
            }
        } else if (cmd == "sort") {
            std::string s_lo, s_hi;
            long lo = 0, hi = 0;
            if (!(words >> s_lo) || !(words >> s_hi) ||
                !parse_number(s_lo, lo) || !parse_number(s_hi, hi)) {
                std::cout << "usage: sort <lo> <hi>\n";
            } else if (lo < 0 || hi < 0 || lo > hi || hi > static_cast<long>(MEM_SIZE)) {
                std::cout << "invalid range [lo, hi)\n";
            } else {
                bubble_sort_mem(mem,
                                static_cast<std::uint16_t>(lo),
                                static_cast<std::uint16_t>(hi));
            }
        } else if (cmd == "reg") {
            std::string reg_name, v;
            long val = 0;
            if (!(words >> reg_name) || !(words >> v) || !parse_number(v, val)) {
                std::cout << "usage: reg <a|b|h|sp|pc> <val>\n";
            } else {
                if (reg_name == "a" || reg_name == "A") {
                    cpu.a = static_cast<Byte>(val & 0xFF);
                } else if (reg_name == "b" || reg_name == "B") {
                    cpu.b = static_cast<Byte>(val & 0xFF);
                } else if (reg_name == "h" || reg_name == "H") {
                    cpu.h = static_cast<std::uint16_t>(val & 0xFFFF);
                } else if (reg_name == "sp" || reg_name == "SP") {
                    cpu.stack.sp = static_cast<std::uint16_t>(val & 0xFFFF);
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