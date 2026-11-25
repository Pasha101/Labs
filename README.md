# ember — 4KB Computer & 8-Bit CPU Simulator

`ember` is an educational virtual machine and 8-bit CPU simulator written in C++17 with strict compiler enforcement (`-Wall -Wextra -Werror` and AddressSanitizer/UBSan in Debug builds on supported toolchains).

---

## Table of Contents
1. [Architecture & Complete Memory Map](#1-architecture--complete-memory-map)
2. [Lab 01 — A Box of Bytes (Memory Infrastructure)](#2-lab-01--a-box-of-bytes-memory-infrastructure)
3. [Lab 02 — Bits Don't Lie (CPU, ALU & Flags)](#3-lab-02--bits-dont-lie-cpu-alu--flags)
4. [Lab 03 — Addresses, Not Names (Pointers & Memory Access)](#4-lab-03--addresses-not-names-pointers--memory-access)
5. [Lab 04 — The Shape of Control (Branches, Loops & Scope)](#5-lab-04--the-shape-of-control-branches-loops--scope)
6. [Lab 05 — Many of One Thing (Arrays, Strings, Search & Screen)](#6-lab-05--many-of-one-thing-arrays-strings-search--screen)
7. [Lab 06 — Named Bundles (Structs, Enums, Heap & Sprites)](#7-lab-06--named-bundles-structs-enums-heap--sprites)
8. [Complete Instruction Set Architecture (ISA)](#8-complete-instruction-set-architecture-isa)
9. [Interactive Console Commands (REPL)](#9-interactive-console-commands-repl)
10. [Build, Execution & Verification](#10-build-execution--verification)

---

## 1. Architecture & Complete Memory Map

`ember` implements a Von Neumann architecture with a flat 4096-byte (`0x000`–`0xFFF`) byte-addressable RAM shared between code, data, video memory, memory-mapped I/O ports, heap, and stack.

### CPU Registers & State
- **`A`, `B` (8-bit):** General-purpose working registers (`0x00`–`0xFF`).
- **`H` (16-bit):** Address register used as a guest pointer (`0x0000`–`0x0FFF`).
- **`PC` (16-bit):** Program Counter pointing to the next instruction; starts at `0x0000`.
- **`Flags` (`Z`, `N`, `C`):** 1-bit status flags updated by ALU operations and `CMP` (and `C` on screen bounds overflow or heap exhaustion).
- **`HALT` (`bool`):** Execution state flag; stops `step` and `run` when `Op::Halt` (`0x00`) or an invalid instruction occurs.
- **`heap_ptr` (16-bit):** Bump-allocator cursor starting at `HEAP_LO` (`0x0C00`), growing upward toward `HEAP_HI` (`0x0EFF`).
- **`ticks` (`std::size_t`):** Total executed guest instructions counter (exposed modulo 256 at `0x0B01`).

### Complete Memory Map (`0x000`–`0xFFF`, 4096 Bytes)

```text
0x000 ┌─────────────────────────────┐
      │  CODE                       │  2048 bytes (instructions; PC starts at 0x000)
0x800 ├─────────────────────────────┤
      │  DATA                       │  512 bytes  (strings, arrays, variables)
0xA00 ├─────────────────────────────┤
      │  VRAM   64 x 32, 1 bit/px   │  256 bytes  (framebuffer inside RAM)
0xB00 ├─────────────────────────────┤
      │  MMIO / RESERVED            │  256 bytes  (0xB00 PORT_OUT, 0xB01 TICKS)
0xC00 ├─────────────────────────────┤
      │  HEAP     ▼ grows upward    │  768 bytes  (ALLOC cuts blocks up from 0xC00)
0xF00 ├─────────────────────────────┤
      │  STACK    ▲ grows downward  │  256 bytes  (SP starts at 0xFFF, Lab 07)
0xFFF └─────────────────────────────┘
```

### Project Structure
- `src/memory.hpp` / `src/memory.cpp`: 4KB RAM array, bounds-checked byte access (`mem_get`, `mem_set`), and Little-Endian 16-bit access (`get16`, `set16`).
- `src/dump.hpp` / `src/dump.cpp`: 16-byte-per-row `dump()` hexdump with ASCII gutter and 4-format `show_byte()` inspector.
- `src/alu.hpp` / `src/alu.cpp`: 8-bit Arithmetic Logic Unit and `Z`/`N`/`C` flag computation.
- `src/cpu.hpp` / `src/cpu.cpp`: `enum class Op`, `struct Cpu`, full `switch (op)` decoder in `step()`, guest bump-allocator (`Op::Alloc`), MMIO port hooks, and register printer.
- `src/screen.hpp` / `src/screen.cpp`: 64×32 monochrome framebuffer logic (`show`, `plot`, `unplot`, `clear`, `draw_frame`) operating directly on `0x0A00`–`0x0AFF`.
- `src/main.cpp`: Interactive REPL shell, `struct Sprite` engine (`Sprite sprites[8]`), struct memory layout inspector (`layout`), and search/sort tools.

---

## 2. Lab 01 — A Box of Bytes (Memory Infrastructure)

### Core Design Decisions
1. **Fixed-Width Integer Types (`Byte = std::uint8_t`):** Guarantees an exact 8-bit unsigned cell (`0..255`, `0x00..0xFF`) across all platforms and compilers, avoiding signed `char` pitfalls.
2. **Unsigned Addressing (`std::size_t`):** Used for memory offsets and `MEM_SIZE = 4096` to prevent negative indexing.
3. **Zero-Initialization & UB Prevention (`Byte data[MEM_SIZE]{};`):** Initializes all 4096 bytes to `0` at construction. Omitting `{}` leaves stack garbage in `data`, causing Undefined Behavior (UB) when `dump()` reads uninitialized memory.
4. **Bounds-Checking Contract:**
   - `mem_get(mem, addr)`: Returns `mem.data[addr]` if `addr < 4096`; safely returns `0` if `addr >= 4096`.
   - `mem_set(mem, addr, val)`: Writes `val` and returns `true` if `addr < 4096`; modifies nothing and returns `false` if out of bounds.
5. **Multi-View Inspection:**
   - `show_byte(b)` prints a single byte in four representations: Decimal, Hexadecimal, 8-bit Binary, and ASCII (printable characters `32..126` shown directly, others as `.`).
   - `dump(mem)` prints all 4096 bytes in rows of 16 bytes (`2^4`), aligning hex offsets with an ASCII sidebar.

---

## 3. Lab 02 — Bits Don't Lie (CPU, ALU & Flags)

### ALU & Flag Semantics
All ALU instructions (`0x10`–`0x1A`) operate on 8-bit registers `A` and `B` with modular 8-bit wrap-around:
- **`Z` (Zero):** Set to `1` when the 8-bit result equals `0x00`; otherwise `0`.
- **`N` (Negative):** Copy of bit 7 (MSB) of the result: `(result & 0x80) != 0`. Represents the sign bit in Two's Complement signed arithmetic (`-128..127`).
- **`C` (Carry / Borrow):**
  - `ADD` (`0x10`): Computed in 16 bits (`uint16_t sum = a + b`); `C = 1` if `sum > 0xFF` (unsigned overflow).
  - `SUB` (`0x11`) / `CMP` (`0x1A`): `C = 1` if `a < b` (unsigned borrow occurred).
  - `SHL` (`0x16`): `C` receives the bit shifted out of bit 7.
  - `SHR` (`0x17`): `C` receives the bit shifted out of bit 0.
  - `AND`, `OR`, `XOR`, `NOT`: Always clear `C = 0`.
  - `INC`, `DEC`: Update `Z` and `N` without modifying `C`.

---

## 4. Lab 03 — Addresses, Not Names (Pointers & Memory Access)

### Little-Endian 16-Bit Storage (`get16` / `set16`)
16-bit words (addresses and immediate 16-bit operands) are stored across two adjacent bytes with boundary check `addr + 1 >= MEM_SIZE`:
- **Low Byte (`val & 0xFF`):** Stored at `addr`
- **High Byte (`(val >> 8) & 0xFF`):** Stored at `addr + 1`

### Theory: Host Pointer vs Guest Address
- **Guest Address (`std::size_t`, `0x0000`–`0x0FFF`):** An integer index/offset within the virtual machine's `Memory::data` array. It always starts at `0x0000`, is completely isolated from the host OS, and remains deterministic across runs and machines.
- **Host Pointer (`Byte*`):** A real 64-bit virtual memory address in physical RAM managed by the host operating system and hardware MMU. Due to ASLR (Address Space Layout Randomization), host pointers change on every execution and cannot be serialized or shared.

---

## 5. Lab 04 — The Shape of Control (Branches, Loops & Scope)

### Control Flow & Safety Architecture
- **Full `switch` Decoder (`step`):** Every defined opcode is matched by an explicit `case`. Any unknown opcode prints `unknown opcode 0x..` and immediately sets `cpu.halted = true`.
- **Flags & Branching Contract:**
  - `LOAD` and `LOADI` **do not modify flags**. Conditional jumps (`JZ`, `JNZ`, `JC`, `JNC`) must follow an ALU instruction or `CMP A, B` (`0x1A`).
  - When a jump (`JMP`, `JZ`, `JNZ`, `JC`, `JNC`) is **taken**, `PC` is set directly to `addr16` without adding the 3-byte instruction length. When **not taken**, `PC += 3`.
- **Infinite Loop Protection (`run`):** Runs inside `for (;;)` with a safety limit of `MAX_STEPS = 100000`, stopping with `step limit (100000) reached` if exceeded.
- **Breakpoints (`b <addr>`):** Executes instructions until `cpu.pc == addr`, `HALT`, or `MAX_STEPS`.

### Listing 1: Countdown `3 2 1` (Guest Bytecode vs C++ `while`)

```text
addr  bytes      instruction
0000  20 03      LOADI A, 3        ; A = 3
0002  03         loop: OUTN        ; print A as decimal + space
0003  19               DEC A       ; A = A - 1 (sets Z = 1 when A reaches 0)
0004  32 02 00         JNZ loop    ; if Z == 0, jump to 0x0002
0007  00               HALT        ; stop execution
```
**Output:** `3 2 1 `

| Step | `PC` (before) | Instruction | `A` (after) | `Z` (after) | Equivalent C++ `while (a > 0)` |
| :---: | :---: | :--- | :---: | :---: | :--- |
| 1 | `0x0000` | `LOADI A, 3` | `3` | `0` | `int a = 3;` |
| 2 | `0x0002` | `OUTN` | `3` | `0` | `std::cout << a << ' ';` (prints `3 `) |
| 3 | `0x0003` | `DEC A` | `2` | `0` | `--a;` (`a` becomes `2`) |
| 4 | `0x0004` | `JNZ 0x0002` | `2` | `0` | `while (a > 0)` -> `true`, jump to `0x0002` |
| 5 | `0x0002` | `OUTN` | `2` | `0` | `std::cout << a << ' ';` (prints `2 `) |
| 6 | `0x0003` | `DEC A` | `1` | `0` | `--a;` (`a` becomes `1`) |
| 7 | `0x0004` | `JNZ 0x0002` | `1` | `0` | `while (a > 0)` -> `true`, jump to `0x0002` |
| 8 | `0x0002` | `OUTN` | `1` | `0` | `std::cout << a << ' ';` (prints `1 `) |
| 9 | `0x0003` | `DEC A` | `0` | `1` | `--a;` (`a` becomes `0`, `Z = 1`) |
| 10 | `0x0004` | `JNZ 0x0002` | `0` | `1` | `while (a > 0)` -> `false`, fall through to `0x0007` |
| 11 | `0x0007` | `HALT` | `0` | `1` | loop finished |

### Listing 2: Guest Linear Search in `[0x0800, 0x0808)`

```text
addr  bytes      instruction
0000  28 00 08   LOADH H, 0x0800     ; H = 0x0800 (start of 8-byte buffer)
0003  29         loop:  LOAD A, [H]  ; A = mem[H]
0004  21 41             LOADI B, 0x41; B = 0x41 (target byte 'A')
0006  1A                CMP A, B     ; compare mem[H] with 0x41
0007  31 16 00          JZ found     ; if equal (Z == 1), jump to 0x0016
000A  2B                INCH         ; H = H + 1
000B  2D                HLOW         ; A = H & 0xFF (current index 0..8)
000C  21 08             LOADI B, 0x08; B = 8 (end offset for 0x0808)
000E  1A                CMP A, B     ; reached 0x0808?
000F  32 03 00          JNZ loop     ; if not (Z == 0), continue loop
0012  20 FF      miss:  LOADI A, 255 ; miss indicator (0xFF)
0014  03                OUTN         ; print 255
0015  00                HALT         ; stop
0016  2D         found: HLOW         ; A = H & 0xFF (matching index 0..7)
0017  03                OUTN         ; print matching index
0018  00                HALT         ; stop
```

---

## 6. Lab 05 — Many of One Thing (Arrays, Strings, Search & Screen)

### Framebuffer Addressing Formula (64×32 Monochrome VRAM)

The 64×32 monochrome display lives directly inside guest RAM at `0x0A00`–`0x0AFF` (256 bytes = 32 rows × 8 bytes per row, 1 bit per pixel, MSB on the left):

```text
byte_addr = 0x0A00 + y * 8 + (x / 8)
bit_index = 7 - (x % 8)
is_lit    = (mem[byte_addr] >> bit_index) & 1
```

### Proof That the Screen Is Memory
1. **Lighting `(0, 0)` Without `PLOT`:** Running `CLS` (`0x40`), `LOADI A, 0x80`, `LOADH H, 0x0A00`, `STORE [H], A`, `SHOW` (`0x43`) renders `#` at `(0, 0)`, and `get 2560` (`0x0A00`) outputs `128  0x80  0b10000000  '.'`.
2. **Border Frame VRAM Dump (`frame` + `dump`):**
   ```text
   0a00: ff ff ff ff ff ff ff ff  80 00 00 00 00 00 00 01  |................|
   0a10: 80 00 00 00 00 00 00 01  80 00 00 00 00 00 00 01  |................|
   ```
   - `0x0A00`–`0x0A07` (`0xff` = `0b11111111`): All 64 pixels in row `y = 0` are lit.
   - `0x0A08` (`0x80` = `0b10000000`): Bit 7 lit (`x = 0`, left border).
   - `0x0A0F` (`0x01` = `0b00000001`): Bit 0 lit (`x = 63`, right border).
3. **C-Strings (`OUTS addr16` `0x04`):** Prints characters until `'\0'` or the 256-byte safety ceiling. Storing `"HELLO\0"` at `0x0800` and running `04 00 08 00` outputs `HELLO`.
4. **Memory-Mapped I/O (`0x0B00` `PORT_OUT` & `0x0B01` `TICKS`):** Writing to `0x0B00` immediately prints the byte as ASCII. Writing the same byte twice emits two characters because two bus write cycles occur; in real C/C++ drivers, MMIO pointers require `volatile` to prevent compiler Dead Store Elimination.

---

## 7. Lab 06 — Named Bundles (Structs, Enums, Heap & Sprites)

### Typed Opcode Enumeration (`enum class Op : Byte`)
Raw opcode constants were refactored into a strongly typed `enum class Op : Byte` in `src/cpu.hpp`. In `step()`, the fetched byte is converted via `Op op = static_cast<Op>(raw_op)` and decoded in `switch (op)`. Unknown opcodes still fall into `default:`, printing `unknown opcode 0x..` and halting the machine, while all Lab 05 programs preserve identical behavior.

### Struct Memory Layout, `sizeof` & Padding (`layout` command)

Inspected via `sizeof` and `offsetof` on 64-bit x86_64 (`alignof(std::size_t) == 8`):

| Struct | Total `sizeof` | Alignment | Field Breakdown (`offsetof` & size) | Padding Analysis |
| :--- | :---: | :---: | :--- | :--- |
| `Flags` | **3 bytes** | 1 byte | `z` (0, 1B), `n` (1, 1B), `c` (2, 1B) | **0 bytes padding** (all fields 1-byte aligned). |
| `Sprite` | **5 bytes** | 1 byte | `x` (0, 1B), `y` (1, 1B), `vx` (2, 1B), `vy` (3, 1B), `alive` (4, 1B) | **0 bytes padding** (`Sprite sprites[8]` occupies `8 * 5 = 40` contiguous bytes). |
| `Cpu` | **32 bytes** | 8 bytes | `pc` (0, 8B), `a` (8, 1B), `b` (9, 1B), `h` (10, 2B), `flags` (12, 3B), `halted` (15, 1B), `heap_ptr` (16, 2B), `ticks` (24, 8B) | **6 bytes padding** at offsets `18..23` so 8-byte `ticks` is aligned to an 8-byte boundary (`24 % 8 == 0`). |

### Sprite Engine (`struct Sprite` & `Sprite sprites[8]`)
- `struct Sprite { std::uint8_t x, y; std::int8_t vx, vy; bool alive; };` stores position, signed velocity, and active state for up to 8 sprites (`Sprite sprites[8]`).
- `sprite <i> <x> <y> <vx> <vy>` initializes sprite `i` and plots its initial pixel.
- `tick [n]` clears VRAM, advances all alive sprites by `(vx, vy)` with elastic reflection at the 64×32 screen boundaries (`0..63`, `0..31`), plots them, and renders the updated frame via `show()`.

### Guest Heap & Bump-Allocator (`Op::Alloc` `0x60`)
- Region `0x0C00`–`0x0EFF` (768 bytes) is managed by `cpu.heap_ptr` (initialized to `HEAP_LO = 0x0C00`).
- Executing `ALLOC` (`0x60`) allocates `A` bytes:
  - If `heap_ptr + A <= 0x0F00`: sets `H = heap_ptr`, advances `heap_ptr += A`, and clears `C = 0`.
  - If out of heap space: sets `C = 1` and leaves `H` unmodified.
- **Two Consecutive Allocations Example:**
  ```text
  addr  bytes      instruction
  0000  20 04      LOADI A, 4          ; request 4 bytes
  0002  60         ALLOC               ; H = 0x0C00, heap_ptr = 0x0C04, C = 0
  0003  20 08      LOADI A, 8          ; request 8 bytes
  0005  60         ALLOC               ; H = 0x0C04, heap_ptr = 0x0C0C, C = 0
  0006  00         HALT
  ```
  There is **no gap** between blocks (`0x0C00..0x0C03` followed immediately by `0x0C04..0x0C0B`) because a bump-allocator tracks only a single top pointer (`heap_ptr`) and stores no per-block headers. Consequently, it cannot `FREE` individual blocks without adding block metadata and a free list.

### Dynamic Memory Lifetime & Sanitizer Analysis (`new` / `delete`)

#### Platform Toolchain Note (Windows MSYS2 UCRT64)
Testing `scratch.cpp` with `-fsanitize=address` on Windows MSYS2 (`x86_64-w64-windows-gnu`) produced the following linker limitation because MSYS2 does not ship the Windows ASan/UBSan runtime import libraries:
```text
C:/msys64/ucrt64/bin/ld: cannot find C:/msys64/ucrt64/lib/clang/22/lib/x86_64-w64-windows-gnu/libclang_rt.asan_dynamic.dll.a: No such file or directory
clang++: error: linker command failed with exit code 1
```
*(In WSL, `g++` was not pre-installed in the container image.)*

#### Three Heap Errors, Sanitizer Diagnostics & Fixes

1. **Memory Leak (`int* p = new int(42);` without `delete p;`):**
   - **Sanitizer output (Linux LeakSanitizer):**
     ```text
     ERROR: LeakSanitizer: detected memory leaks
     Direct leak of 4 byte(s) in 1 object(s) allocated from:
         #0 in operator new(unsigned long)
         #1 in main scratch.cpp:8
     ```
   - **Cause & Fix:** The 4-byte heap allocation remains alive until process exit after the local pointer `p` goes out of scope. **Fix:** Pair every `new` with `delete p;` (or manage ownership with `std::unique_ptr<int>`).

2. **Use-After-Free (`delete p; std::cout << *p;`):**
   - **Sanitizer output (AddressSanitizer):**
     ```text
     ERROR: AddressSanitizer: heap-use-after-free on address 0x602000000010 at pc ...
     READ of size 4 at 0x602000000010 thread T0
         #0 in main scratch.cpp:13
     freed by thread T0 here:
         #0 in operator delete(void*)
         #1 in main scratch.cpp:12
     ```
   - **Cause & Fix:** `delete p` returns the memory to the allocator, turning `p` into a **dangling pointer**. Dereferencing `*p` reads poisoned/reused heap memory. **Fix:** Never access an object after `delete`, and immediately set `p = nullptr;`.

3. **Double Free (`delete p; delete p;`):**
   - **Sanitizer output (AddressSanitizer):**
     ```text
     ERROR: AddressSanitizer: attempting double-free on 0x602000000010 in thread T0:
         #0 in operator delete(void*)
         #1 in main scratch.cpp:18
     freed by thread T0 here:
         #0 in operator delete(void*)
         #1 in main scratch.cpp:17
     ```
   - **Cause & Fix:** Calling `delete` twice on the same heap address corrupts the allocator's free-list metadata. **Fix:** Reset `p = nullptr;` after the first `delete` (`delete nullptr;` is a guaranteed no-op in C++).

---

## 8. Complete Instruction Set Architecture (ISA)

| Opcode | Enum (`Op`) | Mnemonic | Size | Flags | Operation | Lab |
| :---: | :--- | :--- | :---: | :---: | :--- | :---: |
| `0x00` | `Op::Halt` | `HALT` | 1 | — | Stop CPU execution (`cpu.halted = true`) | 2 |
| `0x01` | `Op::Nop` | `NOP` | 1 | — | No operation | 2 |
| `0x02` | `Op::Out` | `OUT` | 1 | — | Print `A` as ASCII character | 3 |
| `0x03` | `Op::Outn` | `OUTN` | 1 | — | Print `A` as decimal integer + space | 3 |
| `0x04` | `Op::Outs` | `OUTS addr16` | 3 | — | Print C-string at `addr16` until `'\0'` (max 256 bytes) | 5 |
| `0x10` | `Op::Add` | `ADD A, B` | 1 | Z N C | `A = A + B` | 2 |
| `0x11` | `Op::Sub` | `SUB A, B` | 1 | Z N C | `A = A - B` | 2 |
| `0x12` | `Op::And` | `AND A, B` | 1 | Z N, C=0 | `A = A & B` | 2 |
| `0x13` | `Op::Or` | `OR A, B` | 1 | Z N, C=0 | `A = A \| B` | 2 |
| `0x14` | `Op::Xor` | `XOR A, B` | 1 | Z N, C=0 | `A = A ^ B` | 2 |
| `0x15` | `Op::Not` | `NOT A` | 1 | Z N, C=0 | `A = ~A` | 2 |
| `0x16` | `Op::Shl` | `SHL A` | 1 | Z N C | `A = A << 1` (`C` = old bit 7) | 2 |
| `0x17` | `Op::Shr` | `SHR A` | 1 | Z N C | `A = A >> 1` (`C` = old bit 0) | 2 |
| `0x18` | `Op::Inc` | `INC A` | 1 | Z N | `A = A + 1` | 2 |
| `0x19` | `Op::Dec` | `DEC A` | 1 | Z N | `A = A - 1` | 2 |
| `0x1A` | `Op::Cmp` | `CMP A, B` | 1 | Z N C | Compute `A - B` for flags, discard result | 4 |
| `0x20` | `Op::LoadiA` | `LOADI A, imm8` | 2 | — | `A = imm8` | 3 |
| `0x21` | `Op::LoadiB` | `LOADI B, imm8` | 2 | — | `B = imm8` | 3 |
| `0x22` | `Op::LoadA` | `LOAD A, [addr16]` | 3 | — | `A = mem[addr16]` (reads `TICKS` at `0x0B01`) | 3, 5 |
| `0x23` | `Op::LoadB` | `LOAD B, [addr16]` | 3 | — | `B = mem[addr16]` | 3 |
| `0x24` | `Op::StoreA` | `STORE [addr16], A` | 3 | — | `mem[addr16] = A` (prints char if `0x0B00`) | 3, 5 |
| `0x25` | `Op::StoreB` | `STORE [addr16], B` | 3 | — | `mem[addr16] = B` | 3 |
| `0x26` | `Op::MovAB` | `MOV A, B` | 1 | — | `A = B` | 3 |
| `0x27` | `Op::MovBA` | `MOV B, A` | 1 | — | `B = A` | 3 |
| `0x28` | `Op::LoadH` | `LOADH H, imm16` | 3 | — | `H = imm16` | 3 |
| `0x29` | `Op::LoadAH` | `LOAD A, [H]` | 1 | — | `A = mem[H]` | 3 |
| `0x2A` | `Op::StoreHA` | `STORE [H], A` | 1 | — | `mem[H] = A` | 3 |
| `0x2B` | `Op::IncH` | `INCH` | 1 | — | `H = H + 1` | 3 |
| `0x2C` | `Op::DecH` | `DECH` | 1 | — | `H = H - 1` | 3 |
| `0x2D` | `Op::HLow` | `HLOW` | 1 | — | `A = H & 0xFF` | 4 |
| `0x30` | `Op::Jmp` | `JMP addr16` | 3 | — | `PC = addr16` | 4 |
| `0x31` | `Op::Jz` | `JZ addr16` | 3 | — | If `Z == 1`, `PC = addr16`; else `PC += 3` | 4 |
| `0x32` | `Op::Jnz` | `JNZ addr16` | 3 | — | If `Z == 0`, `PC = addr16`; else `PC += 3` | 4 |
| `0x33` | `Op::Jc` | `JC addr16` | 3 | — | If `C == 1`, `PC = addr16`; else `PC += 3` | 4 |
| `0x34` | `Op::Jnc` | `JNC addr16` | 3 | — | If `C == 0`, `PC = addr16`; else `PC += 3` | 4 |
| `0x40` | `Op::Cls` | `CLS` | 1 | — | Clear 256 bytes of VRAM (`0x0A00`–`0x0AFF`) | 5 |
| `0x41` | `Op::Plot` | `PLOT` | 1 | C | Set pixel `(x=A, y=B)`; sets `C=1` if out of bounds | 5 |
| `0x42` | `Op::Unplot` | `UNPLOT` | 1 | C | Clear pixel `(x=A, y=B)`; sets `C=1` if out of bounds | 5 |
| `0x43` | `Op::Show` | `SHOW` | 1 | — | Render 64×32 VRAM to terminal | 5 |
| `0x60` | `Op::Alloc` | `ALLOC` | 1 | C | Allocate `A` bytes in heap (`0x0C00`–`0x0EFF`) into `H`; `C=1` if full | 6 |

---

## 9. Interactive Console Commands (REPL)

| Command | Description |
| :--- | :--- |
| `dump` | Print all 4096 bytes of RAM in 16-byte rows with ASCII gutter |
| `get <addr>` | Inspect a single byte in Decimal, Hex, Binary, and ASCII |
| `set <addr> <val>` | Write a byte (`0..255`, dec or `0x` hex) to `addr` |
| `set16 <addr> <val>` | Write a 16-bit Little-Endian word to `addr` and `addr + 1` |
| `find <lo> <hi> <byte>` | Linear search for `<byte>` in `[lo, hi)` |
| `findnz <lo> <hi>` | Linear search for the first non-zero byte in `[lo, hi)` |
| `sort <lo> <hi>` | In-place bubble sort of bytes in `[lo, hi)` |
| `show` | Render the 64×32 monochrome framebuffer (`0x0A00`–`0x0AFF`) |
| `cls` | Clear all 256 bytes of VRAM |
| `frame` | Draw a 1-pixel border frame around the 64×32 screen |
| `plot <x> <y>` | Light pixel `(x, y)` in VRAM (`x` in `0..63`, `y` in `0..31`) |
| `rowmin <y>` | Print leftmost lit pixel `x` in row `y` (`0..31`) |
| `sprite <i> <x> <y> <vx> <vy>` | Configure sprite `i` (`0..7`) at `(x, y)` with velocity `(vx, vy)` |
| `tick [n]` | Advance all active sprites `n` steps (default `1`) with boundary bounce and render |
| `layout` | Print `sizeof`, `alignof`, and field `offsetof` values for `Flags`, `Cpu`, and `Sprite` |
| `regs` | Display `PC`, `A`, `B`, `H`, `HEAP`, `Z`, `N`, `C`, and `HALT` |
| `reg <a\|b\|h\|pc> <val>` | Set register `A`, `B`, `H`, or `PC` directly |
| `step` | Execute a single instruction at `PC` |
| `run` | Execute instructions until `HALT` or `MAX_STEPS` (`100000`) |
| `b <addr>` | Run until `PC == addr`, `HALT`, or `MAX_STEPS` |
| `help` | List all available commands |
| `quit` / `exit` | Exit the simulator |

---

## 10. Build, Execution & Verification

```bash
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
./ember.exe
```

To verify backward compatibility with Lab 05:
```powershell
Get-Content ..\..\checks\lab-05.txt | .\ember.exe
```