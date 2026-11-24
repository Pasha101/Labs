# ember — 4KB Computer & 8-Bit CPU Simulator

`ember` is an educational virtual machine and 8-bit CPU simulator written in C++17 with strict compiler enforcement (`-Wall -Wextra -Werror` and AddressSanitizer/UBSan in Debug builds).

---

## Table of Contents
1. [Architecture & Memory Map](#1-architecture--memory-map)
2. [Lab 01 — A Box of Bytes (Memory Infrastructure)](#2-lab-01--a-box-of-bytes-memory-infrastructure)
3. [Lab 02 — Bits Don't Lie (CPU, ALU & Flags)](#3-lab-02--bits-dont-lie-cpu-alu--flags)
4. [Lab 03 — Addresses, Not Names (Pointers & Memory Access)](#4-lab-03--addresses-not-names-pointers--memory-access)
5. [Lab 04 — The Shape of Control (Branches, Loops & Scope)](#5-lab-04--the-shape-of-control-branches-loops--scope)
6. [Lab 05 — Many of One Thing (Arrays, Strings, Search & Screen)](#6-lab-05--many-of-one-thing-arrays-strings-search--screen)
7. [Complete Instruction Set Architecture (ISA)](#7-complete-instruction-set-architecture-isa)
8. [Interactive Console Commands (REPL)](#8-interactive-console-commands-repl)
9. [Build, Execution & Verification](#9-build-execution--verification)

---

## 1. Architecture & Memory Map

`ember` implements a Von Neumann architecture with a flat 4096-byte (`0x000`–`0xFFF`) byte-addressable RAM shared between code, data, video memory, and memory-mapped I/O ports.

### CPU Registers & State
- **`A`, `B` (8-bit):** General-purpose working registers (`0x00`–`0xFF`).
- **`H` (16-bit):** Address register used as a guest pointer (`0x0000`–`0x0FFF`).
- **`PC` (16-bit):** Program Counter pointing to the next instruction; starts at `0x0000`.
- **`Flags` (`Z`, `N`, `C`):** 1-bit status flags updated by ALU operations and `CMP` (and `C` on screen bounds overflow).
- **`HALT` (`bool`):** Execution state flag; stops `step` and `run` when `HALT` (`0x00`) or an invalid instruction occurs.
- **`ticks` (`std::size_t`):** Total executed guest instructions counter (exposed modulo 256 at `0x0B01`).

### Memory Map (`0x000`–`0xFFF`, 4096 Bytes)

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
      │  HEAP                       │  768 bytes  (dynamic memory region)
0xF00 ├─────────────────────────────┤
      │  STACK                      │  256 bytes  (grows downward from 0xFFF)
0xFFF └─────────────────────────────┘
```

### Project Structure
- `src/memory.hpp` / `src/memory.cpp`: 4KB RAM array, bounds-checked byte access (`mem_get`, `mem_set`), and Little-Endian 16-bit access (`get16`, `set16`).
- `src/dump.hpp` / `src/dump.cpp`: 16-byte-per-row `dump()` hexdump with ASCII gutter and 4-format `show_byte()` inspector.
- `src/alu.hpp` / `src/alu.cpp`: 8-bit Arithmetic Logic Unit and `Z`/`N`/`C` flag computation.
- `src/cpu.hpp` / `src/cpu.cpp`: CPU state, full `switch (opcode)` decoder in `step()`, MMIO port hooks, and register printer.
- `src/screen.hpp` / `src/screen.cpp`: 64×32 monochrome framebuffer logic (`show`, `plot`, `unplot`, `clear`, `draw_frame`) operating directly on `0x0A00`–`0x0AFF`.
- `src/main.cpp`: Interactive REPL shell (`dump`, `get`, `set`, `set16`, `find`, `findnz`, `sort`, `show`, `cls`, `frame`, `plot`, `rowmin`, `regs`, `reg`, `step`, `run`, `b`, `help`, `quit`).

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

### Example Inspection Session
```text
ember> set 0 65
ember> set 1 0x42
ember> get 0
65  0x41  0b01000001  'A'
ember> get 1
66  0x42  0b01000010  'B'
```

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

### Worked Arithmetic Traces
- **Unsigned Overflow (`200 + 100`):** `A = 0xC8 (200)`, `B = 0x64 (100)`. `ADD A, B` yields `300 (0x012C)`, truncated to `A = 0x2C (44)` with `Z = 0, N = 0, C = 1`.
- **Two's Complement Negative Result (`10 - 20`):** `A = 0x0A (10)`, `B = 0x14 (20)`. `SUB A, B` wraps to `A = 0xF6 (246 unsigned / -10 signed)` with `Z = 0, N = 1, C = 1`.

---

## 4. Lab 03 — Addresses, Not Names (Pointers & Memory Access)

### Little-Endian 16-Bit Storage (`get16` / `set16`)
16-bit words (addresses and immediate 16-bit operands) are stored across two adjacent bytes with bounds check `addr + 1 >= MEM_SIZE`:
- **Low Byte (`val & 0xFF`):** Stored at `addr`
- **High Byte (`(val >> 8) & 0xFF`):** Stored at `addr + 1`

For example, `LOAD A, [0x0800]` (`0x22`) at `0x0000` is encoded in memory as `22 00 08`.

### Address Register `H` & I/O
- Register `H` (`0x0000`–`0x0FFF`) enables indirect addressing (`LOADH H, imm16`, `LOAD A, [H]`, `STORE [H], A`, `INCH`, `DECH`, `HLOW`), allowing programs to traverse arrays and buffers dynamically.
- Output instructions: `OUT` (`0x02`) prints `A` as an ASCII character; `OUTN` (`0x03`) prints `A` as a decimal integer followed by a space.

### Theory: Host Pointer vs Guest Address
- **Guest Address (`std::size_t`, `0x0000`–`0x0FFF`):** An integer index/offset within the virtual machine's `Memory::data` array. It always starts at `0x0000`, is completely isolated from the host OS, and remains deterministic across runs and machines.
- **Host Pointer (`Byte*`):** A real 64-bit virtual memory address in physical RAM managed by the host operating system and hardware MMU (e.g., `&mem.data[0x0800]` -> `0x00007ff6a41c2800`). Due to ASLR (Address Space Layout Randomization), host pointers change on every execution and cannot be serialized or shared.

### Worked Trace: Code at `0x0000`, Data at `0x0800`
```text
addr  bytes      instruction
0000  22 00 08   LOAD A, [0x0800]    ; load byte from data region 0x0800 ('H' = 0x48)
0003  02         OUT                 ; print 'H'
0004  00         HALT                ; stop execution
```

---

## 5. Lab 04 — The Shape of Control (Branches, Loops & Scope)

### Control Flow & Safety Architecture
- **Full `switch` Decoder (`step`):** Every defined opcode is matched by an explicit `case`. Any unknown opcode prints `unknown opcode 0x..` and immediately sets `cpu.halted = true`.
- **Flags & Branching Contract:**
  - `LOAD` and `LOADI` **do not modify flags**. Conditional jumps (`JZ`, `JNZ`, `JC`, `JNC`) must follow an ALU instruction or `CMP A, B` (`0x1A`).
  - When a jump (`JMP`, `JZ`, `JNZ`, `JC`, `JNC`) is **taken**, `PC` is set directly to `addr16` without adding the 3-byte instruction length. When **not taken**, `PC += 3`.
- **Infinite Loop Protection (`run`):** `run` runs inside `for (;;)` with a safety limit of `MAX_STEPS = 100000`. If a broken `JMP` creates an infinite loop, execution stops and prints `step limit (100000) reached`.
- **Breakpoints (`b <addr>`):** Executes instructions until `cpu.pc == addr`, `HALT`, or `MAX_STEPS`.

### Listing 1: Countdown `3 2 1` (Guest Bytecode vs C++ `while`)

#### Machine Code Listing (`0x0000`–`0x0007`)
```text
addr  bytes      instruction
0000  20 03      LOADI A, 3        ; A = 3
0002  03         loop: OUTN        ; print A as decimal + space
0003  19               DEC A       ; A = A - 1 (sets Z = 1 when A reaches 0)
0004  32 02 00         JNZ loop    ; if Z == 0, jump to 0x0002
0007  00               HALT        ; stop execution
```
**Output:** `3 2 1 `

#### Execution Trace Side-by-Side with C++ `while (a > 0)`

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

Searches for byte `0x41` (`'A'`) across an 8-byte buffer starting at `0x0800`:
- **How the loop detects reaching `hi` (`0x0808`):** After incrementing `H` via `INCH`, `HLOW` copies the low byte of `H` (`0x00`..`0x08`) into `A` and compares it with `0x08` using `CMP A, B`. When `H` reaches `0x0808`, `Z` becomes `1` and `JNZ loop` falls through to `miss`.
- **How match vs miss is reported:** If `mem[H] == 0x41`, `JZ found` jumps to `0x0016`, where `HLOW` + `OUTN` prints the matching offset (`0..7`). If the buffer ends without a match, execution reaches `miss` (`0x0012`), which loads `255` (`0xFF`) into `A` and prints `255` via `OUTN`.

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

The display is not a separate host array; it lives inside guest RAM at `0x0A00`–`0x0AFF` (256 bytes = 32 rows × 8 bytes per row, 1 bit per pixel, MSB on the left):

```text
byte_addr = 0x0A00 + y * 8 + (x / 8)
bit_index = 7 - (x % 8)
is_lit    = (mem[byte_addr] >> bit_index) & 1
```

- **Set pixel (`plot`):** `mem[byte_addr] |= (1u << bit_index)` (rejects `x < 0 || x >= 64 || y < 0 || y >= 32`; opcode `PLOT` sets `C = 1` on out-of-bounds).
- **Clear pixel (`unplot`):** `mem[byte_addr] &= ~(1u << bit_index)`.

### Proof That the Screen Is Memory

#### 1. Lighting Top-Left Pixel `(0, 0)` Without `PLOT`
```text
addr  bytes      instruction
0000  40         CLS                 ; clear 256 bytes at 0x0A00..0x0AFF
0001  20 80      LOADI A, 0x80       ; 0x80 = 0b10000000 (bit 7 = leftmost pixel)
0003  28 00 0A   LOADH H, 0x0A00     ; point H to start of VRAM
0006  2A         STORE [H], A        ; write directly to RAM without PLOT
0007  43         SHOW                ; render VRAM to terminal
0008  00         HALT
```
Running this program renders `#` at `(0, 0)`, and `get 2560` (`0x0A00`) outputs `128  0x80  0b10000000  '.'`.

#### 2. Three-Pixel Guest Program (`(2, 1)`, `(10, 5)`, `(63, 31)`)
```text
addr  bytes      instruction
0000  40         CLS                 ; clear VRAM
0001  20 02      LOADI A, 2          ; x = 2
0003  21 01      LOADI B, 1          ; y = 1
0005  41         PLOT                ; plot (2, 1)
0006  20 0A      LOADI A, 10         ; x = 10
0008  21 05      LOADI B, 5          ; y = 5
000A  41         PLOT                ; plot (10, 5)
000B  20 3F      LOADI A, 63         ; x = 63
000D  21 1F      LOADI B, 31         ; y = 31
000F  41         PLOT                ; plot (63, 31)
0010  43         SHOW                ; render screen
0011  00         HALT
```

#### 3. Border Frame (`frame`) & VRAM Dump (`0x0A00`–`0x0A1F`)
Running `frame` draws a 1-pixel border around the 64×32 screen. Inspecting `dump` at `0x0A00` shows:
```text
0a00: ff ff ff ff ff ff ff ff  80 00 00 00 00 00 00 01  |................|
0a10: 80 00 00 00 00 00 00 01  80 00 00 00 00 00 00 01  |................|
```
- **`0x0A00`–`0x0A07` (`0xff`):** Each `0xFF` (`0b11111111`) lights 8 consecutive horizontal pixels, forming the 64-pixel top edge (`y = 0`).
- **`0x0A08` (`0x80`):** `0b10000000` lights bit 7 (`x = 0`, left border of row `y = 1`).
- **`0x0A0F` (`0x01`):** `0b00000001` lights bit 0 (`x = 63`, right border of row `y = 1`).

### C-Strings (`OUTS`), Search & In-Place Sort
- **`OUTS addr16` (`0x04`):** Prints characters from `addr16` until `'\0'` (`0x00`) or the 256-byte safety ceiling (protecting against unterminated guest strings).
  - Storing `"HELLO\0"` (`48 45 4C 4C 4F 00`) at `0x0800` and running `04 00 08 00` outputs `HELLO`.
- **Linear Search Across Data & VRAM:**
  - Data region: `find 0x800 0x810 0x4C` -> `found at 0x0802 (2050)` (first `'L'` in `"HELLO"`).
  - VRAM region: `findnz 0xA00 0xB00` -> `found at 0x0a00 (2560)` (first lit byte in VRAM).
- **In-Place Bubble Sort (`sort <lo> <hi>`):** Sorts bytes in `[lo, hi)` directly inside guest memory.

### Memory-Mapped I/O (`0x0B00` `PORT_OUT` & `0x0B01` `TICKS`)
- Writing to `0x0B00` (`PORT_OUT`) from guest code immediately prints the written byte as an ASCII character while also storing it in memory. Reading `0x0B01` (`TICKS`) returns executed instruction count modulo 256.
- Program `20 48 24 00 0B 20 49 24 00 0B 00` prints `HI` using only `STORE [0x0B00], A`, and `get 2816` (`0x0B00`) shows `73` (`'I'`).
- **Double Write & `volatile`:** Writing `0x41` twice to `0x0B00` executes two guest `STORE` cycles and prints `AA`. In real C/C++ hardware drivers, MMIO addresses must be qualified with `volatile` (`volatile std::uint8_t*`); otherwise an optimizing compiler eliminates the first of two consecutive writes to the same address (Dead Store Elimination).

---

## 7. Complete Instruction Set Architecture (ISA)

| Opcode | Mnemonic | Size | Flags | Operation | Lab |
| :---: | :--- | :---: | :---: | :--- | :---: |
| `0x00` | `HALT` | 1 | — | Stop CPU execution (`cpu.halted = true`) | 2 |
| `0x01` | `NOP` | 1 | — | No operation | 2 |
| `0x02` | `OUT` | 1 | — | Print `A` as ASCII character | 3 |
| `0x03` | `OUTN` | 1 | — | Print `A` as decimal integer + space | 3 |
| `0x04` | `OUTS addr16` | 3 | — | Print C-string at `addr16` until `'\0'` (max 256 bytes) | 5 |
| `0x10` | `ADD A, B` | 1 | Z N C | `A = A + B` | 2 |
| `0x11` | `SUB A, B` | 1 | Z N C | `A = A - B` | 2 |
| `0x12` | `AND A, B` | 1 | Z N, C=0 | `A = A & B` | 2 |
| `0x13` | `OR A, B` | 1 | Z N, C=0 | `A = A \| B` | 2 |
| `0x14` | `XOR A, B` | 1 | Z N, C=0 | `A = A ^ B` | 2 |
| `0x15` | `NOT A` | 1 | Z N, C=0 | `A = ~A` | 2 |
| `0x16` | `SHL A` | 1 | Z N C | `A = A << 1` (`C` = old bit 7) | 2 |
| `0x17` | `SHR A` | 1 | Z N C | `A = A >> 1` (`C` = old bit 0) | 2 |
| `0x18` | `INC A` | 1 | Z N | `A = A + 1` | 2 |
| `0x19` | `DEC A` | 1 | Z N | `A = A - 1` | 2 |
| `0x1A` | `CMP A, B` | 1 | Z N C | Compute `A - B` for flags, discard result | 4 |
| `0x20` | `LOADI A, imm8` | 2 | — | `A = imm8` | 3 |
| `0x21` | `LOADI B, imm8` | 2 | — | `B = imm8` | 3 |
| `0x22` | `LOAD A, [addr16]` | 3 | — | `A = mem[addr16]` (reads `TICKS` at `0x0B01`) | 3, 5 |
| `0x23` | `LOAD B, [addr16]` | 3 | — | `B = mem[addr16]` | 3 |
| `0x24` | `STORE [addr16], A` | 3 | — | `mem[addr16] = A` (prints char if `0x0B00`) | 3, 5 |
| `0x25` | `STORE [addr16], B` | 3 | — | `mem[addr16] = B` | 3 |
| `0x26` | `MOV A, B` | 1 | — | `A = B` | 3 |
| `0x27` | `MOV B, A` | 1 | — | `B = A` | 3 |
| `0x28` | `LOADH H, imm16` | 3 | — | `H = imm16` | 3 |
| `0x29` | `LOAD A, [H]` | 1 | — | `A = mem[H]` | 3 |
| `0x2A` | `STORE [H], A` | 1 | — | `mem[H] = A` | 3 |
| `0x2B` | `INCH` | 1 | — | `H = H + 1` | 3 |
| `0x2C` | `DECH` | 1 | — | `H = H - 1` | 3 |
| `0x2D` | `HLOW` | 1 | — | `A = H & 0xFF` | 4 |
| `0x30` | `JMP addr16` | 3 | — | `PC = addr16` | 4 |
| `0x31` | `JZ addr16` | 3 | — | If `Z == 1`, `PC = addr16`; else `PC += 3` | 4 |
| `0x32` | `JNZ addr16` | 3 | — | If `Z == 0`, `PC = addr16`; else `PC += 3` | 4 |
| `0x33` | `JC addr16` | 3 | — | If `C == 1`, `PC = addr16`; else `PC += 3` | 4 |
| `0x34` | `JNC addr16` | 3 | — | If `C == 0`, `PC = addr16`; else `PC += 3` | 4 |
| `0x40` | `CLS` | 1 | — | Clear 256 bytes of VRAM (`0x0A00`–`0x0AFF`) | 5 |
| `0x41` | `PLOT` | 1 | C | Set pixel `(x=A, y=B)`; sets `C=1` if out of bounds | 5 |
| `0x42` | `UNPLOT` | 1 | C | Clear pixel `(x=A, y=B)`; sets `C=1` if out of bounds | 5 |
| `0x43` | `SHOW` | 1 | — | Render 64×32 VRAM to terminal | 5 |

---

## 8. Interactive Console Commands (REPL)

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
| `regs` | Display `PC`, `A`, `B`, `H`, `Z`, `N`, `C`, and `HALT` |
| `reg <a\|b\|h\|pc> <val>` | Set register `A`, `B`, `H`, or `PC` directly |
| `step` | Execute a single instruction at `PC` |
| `run` | Execute instructions until `HALT` or `MAX_STEPS` (`100000`) |
| `b <addr>` | Run until `PC == addr`, `HALT`, or `MAX_STEPS` |
| `help` | List all available commands |
| `quit` / `exit` | Exit the simulator |

---

## 9. Build, Execution & Verification

```bash
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
./ember.exe
```

To run the automated Lab 05 verification script[cite: 9]:
```powershell
Get-Content ..\..\checks\lab-05.txt | .\ember.exe
```