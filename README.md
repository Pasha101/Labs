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
8. [Lab 07 — Call & Return (Functions, Stack & Recursion)](#8-lab-07--call--return-functions-stack--recursion)
9. [Complete Instruction Set Architecture (ISA)](#9-complete-instruction-set-architecture-isa)
10. [Interactive Console Commands (REPL)](#10-interactive-console-commands-repl)
11. [Build, Execution & Verification](#11-build-execution--verification)

---

## 1. Architecture & Complete Memory Map

`ember` implements a Von Neumann architecture with a flat 4096-byte (`0x000`–`0xFFF`) byte-addressable RAM shared between code, data, video memory, memory-mapped I/O ports, heap, and stack.

### CPU Registers & State
- **`A`, `B` (8-bit):** General-purpose working registers (`0x00`–`0xFF`).
- **`H` (16-bit):** Address register used as a guest pointer (`0x0000`–`0x0FFF`).
- **`SP` (16-bit):** Stack Pointer inside `Stack` ADT (`0x0F00`–`0x0FFF`); empty descending stack starting at `0x0FFF`.
- **`PC` (16-bit):** Program Counter pointing to the next instruction; starts at `0x0000`.
- **`Flags` (`Z`, `N`, `C`):** 1-bit status flags updated by ALU operations and `CMP` (and `C` on screen bounds overflow or heap exhaustion).
- **`HALT` (`bool`):** Execution state flag; stops `step` and `run` when `Op::Halt` (`0x00`), stack error, or an invalid instruction occurs.
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
      │  STACK    ▲ grows downward  │  256 bytes  (SP starts at 0xFFF, empty descending)
0xFFF └─────────────────────────────┘
```

### Project Translation Units (`CMakeLists.txt`)
- `src/memory.hpp` / `src/memory.cpp`: 4KB RAM array, bounds-checked byte access (`mem_get`, `mem_set`), and Little-Endian 16-bit access (`get16`, `set16`).
- `src/dump.hpp` / `src/dump.cpp`: 16-byte-per-row `dump()` hexdump with ASCII gutter and 4-format `show_byte()` inspector.
- `src/alu.hpp` / `src/alu.cpp`: 8-bit Arithmetic Logic Unit (`ADD`, `SUB`, `MUL`, bitwise/shifts) and `Z`/`N`/`C` flag computation.
- `src/stack.hpp` / `src/stack.cpp`: Encapsulated `Stack` ADT (`stack_push`, `stack_pop`, `stack_empty`, `stack_full`, `print_stack`) operating over `0x0F00`–`0x0FFF`.
- `src/cpu.hpp` / `src/cpu.cpp`: `enum class Op`, `struct Cpu`, full `switch (op)` decoder in `step()`, guest bump-allocator (`Op::Alloc`), `CALL`/`RET`/`PUSH`/`POP`, and MMIO port hooks.
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
All ALU instructions (`0x10`–`0x1B`) operate on 8-bit registers `A` and `B` with modular 8-bit wrap-around:
- **`Z` (Zero):** Set to `1` when the 8-bit result equals `0x00`; otherwise `0`.
- **`N` (Negative):** Copy of bit 7 (MSB) of the result: `(result & 0x80) != 0`. Represents the sign bit in Two's Complement signed arithmetic (`-128..127`).
- **`C` (Carry / Borrow):**
  - `ADD` (`0x10`): Computed in 16 bits (`uint16_t sum = a + b`); `C = 1` if `sum > 0xFF` (unsigned overflow).
  - `SUB` (`0x11`) / `CMP` (`0x1A`): `C = 1` if `a < b` (unsigned borrow occurred).
  - `MUL` (`0x1B`): Computed in 16 bits (`uint16_t prod = a * b`); `C = 1` if `prod > 0xFF` (product exceeds 8 bits).
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
Raw opcode constants were refactored into a strongly typed `enum class Op : Byte` in `src/cpu.hpp`. In `step()`, the fetched byte is converted via `Op op = static_cast<Op>(raw_op)` and decoded in `switch (op)`.

### Struct Memory Layout, `sizeof` & Padding (`layout` command)

Inspected via `sizeof` and `offsetof` on 64-bit x86_64 (`alignof(std::size_t) == 8`):

| Struct | Total `sizeof` | Alignment | Field Breakdown (`offsetof` & size) | Padding Analysis |
| :--- | :---: | :---: | :--- | :--- |
| `Flags` | **3 bytes** | 1 byte | `z` (0, 1B), `n` (1, 1B), `c` (2, 1B) | **0 bytes padding** (all fields 1-byte aligned). |
| `Stack` | **2 bytes** | 2 bytes | `sp` (0, 2B) | **0 bytes padding**. |
| `Sprite` | **5 bytes** | 1 byte | `x` (0, 1B), `y` (1, 1B), `vx` (2, 1B), `vy` (3, 1B), `alive` (4, 1B) | **0 bytes padding** (`Sprite sprites[8]` occupies `8 * 5 = 40` contiguous bytes). |
| `Cpu` | **32 bytes** | 8 bytes | `pc` (0, 8B), `a` (8, 1B), `b` (9, 1B), `h` (10, 2B), `stack` (12, 2B), `flags` (14, 3B), `halted` (17, 1B), `heap_ptr` (18, 2B), `ticks` (24, 8B) | **4 bytes padding** at offsets `20..23` so 8-byte `ticks` is aligned to an 8-byte boundary (`24 % 8 == 0`). |

### Sprite Engine (`struct Sprite` & `Sprite sprites[8]`)
- `sprite <i> <x> <y> <vx> <vy>` initializes sprite `i` (`0..7`) and plots its initial pixel.
- `tick [n]` clears VRAM, advances all alive sprites by `(vx, vy)` with elastic reflection at the 64×32 screen boundaries (`0..63`, `0..31`), plots them, and renders via `show()`.

### Guest Heap & Bump-Allocator (`Op::Alloc` `0x60`)
- Region `0x0C00`–`0x0EFF` (768 bytes) is managed by `cpu.heap_ptr` (starts at `0x0C00`).
- `ALLOC` (`0x60`) allocates `A` bytes into `H` and advances `heap_ptr += A` (or sets `C = 1` if out of space). Two consecutive allocations of 4 and 8 bytes produce contiguous blocks `0x0C00..0x0C03` and `0x0C04..0x0C0B` (`heap_ptr = 0x0C0C`) with no gap because a bump-allocator stores no block headers.

### Dynamic Memory Lifetime & Sanitizer Analysis (`new` / `delete`)
On Windows MSYS2 (`x86_64-w64-windows-gnu`), linking `-fsanitize=address` reports `cannot find libclang_rt.asan_dynamic.dll.a` because the MSYS2 GNU target does not bundle the ASan runtime DLLs. Below are the three heap lifetime errors, standard sanitizer diagnostics, and fixes:
1. **Memory Leak (`new int(42)` without `delete`):** `ERROR: LeakSanitizer: detected memory leaks` — heap block remains allocated after pointer goes out of scope. **Fix:** pair every `new` with `delete` (or `std::unique_ptr`).
2. **Use-After-Free (`delete p; std::cout << *p;`):** `ERROR: AddressSanitizer: heap-use-after-free` — reading through a dangling pointer after deallocation. **Fix:** set `p = nullptr;` immediately after `delete p;` and never dereference freed pointers.
3. **Double Free (`delete p; delete p;`):** `ERROR: AddressSanitizer: attempting double-free` — freeing the same heap address twice corrupts allocator metadata. **Fix:** reset `p = nullptr;` after the first `delete` (`delete nullptr;` is a safe no-op).

---

## 8. Lab 07 — Call & Return (Functions, Stack & Recursion)

### Stack ADT (`src/stack.hpp` / `src/stack.cpp`)
- **Empty Descending Stack (`0x0F00`–`0x0FFF`, 256 bytes):** `SP` starts at `0x0FFF` (pointing to the next free byte) and grows downward on `push`:
  - `stack_push(s, mem, val)`: If `s.sp < STACK_LO` (`0x0F00`), returns `false` (overflow); otherwise writes `val` to `mem[s.sp]` and decrements `s.sp`.
  - `stack_pop(s, mem, out)`: If `s.sp >= STACK_HI` (`0x0FFF`), returns `false` (underflow); otherwise increments `s.sp` and reads `mem[s.sp]`.
- Any overflow or underflow halts the CPU cleanly with `stack overflow` or `stack underflow` without process crashes or wrap-around.

### Calling Convention (Own Words)
1. **Argument Passing:** The caller places the single 8-bit function argument into register **`A`** before executing `CALL`.
2. **Return Value:** The callee leaves its 8-bit result in register **`A`** before executing `RET`.
3. **Caller-Saved Registers (`B` and `H`):** Registers `B` and `H` are **caller-saved** (volatile across calls). A function may freely overwrite `B` and `H`; if the caller still needs their values or its own `A` after a nested `CALL`, the caller must save them on the stack (`PUSH A` / `PUSH B` / `PUSH H`) before `CALL` and restore them (`POP`) after `RET`.
4. **Return Address Order:** `CALL addr16` pushes the **low byte** of `PC + 3` first, then the **high byte**, and jumps to `addr16`. `RET` pops the **high byte** first, then the **low byte**, and restores `PC`.

### Single-Level `CALL` / `RET` Trace (`CALL printA` -> `OUT` -> `RET` -> `HALT`)

```text
addr  bytes      instruction
0000  20 41      LOADI A, 0x41       ; A = 'A'
0002  54 06 00   CALL printA         ; push return address 0x0005 (lo=0x05, hi=0x00), jump to 0x0006
0005  00         HALT                ; stop after return
0006  02         printA: OUT         ; print A ('A')
0007  55                 RET         ; pop hi=0x00, lo=0x05 -> PC = 0x0005
```

| Step | `PC` (before) | Instruction | `SP` (after) | Stack Bytes (`[0x0FFE]`, `[0x0FFF]`) | Notes |
| :---: | :---: | :--- | :---: | :---: | :--- |
| 1 | `0x0000` | `LOADI A, 0x41` | `0x0FFF` | `—`, `—` | `A = 0x41` (`'A'`), stack empty |
| 2 | `0x0002` | `CALL 0x0006` | `0x0FFD` | `0x00`, `0x05` | Pushes `0x05` at `0x0FFF`, `0x00` at `0x0FFE`; `PC = 0x0006` |
| 3 | `0x0006` | `OUT` | `0x0FFD` | `0x00`, `0x05` | Prints `'A'`, `PC = 0x0007` |
| 4 | `0x0007` | `RET` | `0x0FFF` | `—`, `—` | Pops `0x00` (`hi`) and `0x05` (`lo`); `PC = 0x0005` |
| 5 | `0x0005` | `HALT` | `0x0FFF` | `—`, `—` | Execution halts cleanly |

- **Why `CALL` pushes `PC + 3` instead of `PC`:** `CALL addr16` is a 3-byte instruction (`0x54` + 2-byte address). Pushing `PC` (`0x0002`) would make `RET` jump back onto the `CALL` instruction itself, causing an infinite call loop until stack overflow. Pushing `PC + 3` (`0x0005`) resumes execution at the instruction immediately following `CALL`.

### Recursive Factorial `fact(5)` Listing & Call-Depth Trace

```text
addr  bytes      instruction
0000  20 05      LOADI A, 5          ; argument n = 5
0002  54 07 00   CALL fact           ; A = fact(5)
0005  03         OUTN                ; prints 120
0006  00         HALT
0007  21 00      fact: LOADI B, 0    ; check n == 0
0009  1A               CMP A, B
000A  31 1B 00         JZ base
000D  21 01            LOADI B, 1    ; check n == 1
000F  1A               CMP A, B
0010  31 1B 00         JZ base
0013  50               PUSH A        ; save caller's n on stack (3 bytes/frame with return addr)
0014  19               DEC A         ; A = n - 1
0015  54 07 00         CALL fact     ; recursive call: A = fact(n - 1)
0018  53               POP B         ; restore saved n into B
0019  1B               MUL A, B      ; A = fact(n - 1) * n
001A  55               RET
001B  20 01      base: LOADI A, 1    ; base case returns 1
001D  55               RET
```

#### Call/Return Depth Trace (`fact(5)` -> `120` in 60 steps, max depth = 14 bytes)

| Event | Call Frame | `PC` (after) | `SP` (after) | `A` (after) | Stack Depth |
| :--- | :--- | :---: | :---: | :---: | :---: |
| `CALL fact` (from `main`) | `fact(5)` entered | `0x0007` | `0x0FFD` | `5` | 2 bytes (`0x0005`) |
| `PUSH A` + `CALL fact` | `fact(4)` entered | `0x0007` | `0x0FFA` | `4` | 5 bytes (`+5`, `+0x0018`) |
| `PUSH A` + `CALL fact` | `fact(3)` entered | `0x0007` | `0x0FF7` | `3` | 8 bytes (`+4`, `+0x0018`) |
| `PUSH A` + `CALL fact` | `fact(2)` entered | `0x0007` | `0x0FF4` | `2` | 11 bytes (`+3`, `+0x0018`) |
| `PUSH A` + `CALL fact` | `fact(1)` entered (base) | `0x0007` | `0x0FF1` | `1` | **14 bytes** (`+2`, `+0x0018`) |
| `RET` (from `fact(1)`) | back in `fact(2)` | `0x0018` | `0x0FF3` | `1` | 12 bytes |
| `POP B` + `MUL` + `RET` | back in `fact(3)` | `0x0018` | `0x0FF6` | `2` | 9 bytes (`1 * 2 = 2`) |
| `POP B` + `MUL` + `RET` | back in `fact(4)` | `0x0018` | `0x0FF9` | `6` | 6 bytes (`2 * 3 = 6`) |
| `POP B` + `MUL` + `RET` | back in `fact(5)` | `0x0018` | `0x0FFC` | `24` | 3 bytes (`6 * 4 = 24`) |
| `POP B` + `MUL` + `RET` | back in `main` | `0x0005` | `0x0FFF` | `120` | 0 bytes (`24 * 5 = 120`) |

### Base Cases, `fact(6)` Overflow & Intentional Stack Overflow
- **Base Cases (`fact(0)` and `fact(1)`):** Changing byte `1` from `5` to `0` or `1` jumps immediately to `base` (`0x001B`), printing **`1 `** with `SP = 0x0FFF`.
- **Modified Listing (`fact(6)`):** Changing byte `1` from `5` to `6` computes `6! = 720 (0x02D0)`. Because register `A` is 8-bit, the result wraps modulo 256 to `720 & 0xFF = 208 (0xD0)`, printing **`208 `** while `alu_mul` sets Carry flag **`C = 1`** to signal unsigned 8-bit overflow.
- **Intentional Stack Overflow:** Setting `reg sp 0x0f02` before running `fact(5)` leaves only 3 bytes of stack space (`0x0F00`–`0x0F02`). On the recursive `CALL` inside `fact(5)`, `SP` drops below `STACK_LO` (`0x0F00`), and the VM halts cleanly with:
  ```text
  stack overflow on CALL at PC=0x0015
  ```

---

## 9. Complete Instruction Set Architecture (ISA)

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
| `0x1B` | `Op::Mul` | `MUL A, B` | 1 | Z N C | `A = (A * B) & 0xFF`; `C = 1` if `A * B > 255` | 7 |
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
| `0x50` | `Op::PushA` | `PUSH A` | 1 | — | Push `A` onto stack (`mem[SP--] = A`) | 7 |
| `0x51` | `Op::PushB` | `PUSH B` | 1 | — | Push `B` onto stack (`mem[SP--] = B`) | 7 |
| `0x52` | `Op::PopA` | `POP A` | 1 | — | Pop top of stack into `A` (`A = mem[++SP]`) | 7 |
| `0x53` | `Op::PopB` | `POP B` | 1 | — | Pop top of stack into `B` (`B = mem[++SP]`) | 7 |
| `0x54` | `Op::Call` | `CALL addr16` | 3 | — | Push low then high byte of `PC + 3`, jump to `addr16` | 7 |
| `0x55` | `Op::Ret` | `RET` | 1 | — | Pop high then low byte into `PC` | 7 |
| `0x56` | `Op::PushH` | `PUSH H` | 1 | — | Push low then high byte of `H` onto stack | 7 |
| `0x57` | `Op::PopH` | `POP H` | 1 | — | Pop high then low byte from stack into `H` | 7 |
| `0x60` | `Op::Alloc` | `ALLOC` | 1 | C | Allocate `A` bytes in heap (`0x0C00`–`0x0EFF`) into `H`; `C=1` if full | 6 |

---

## 10. Interactive Console Commands (REPL)

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
| `layout` | Print `sizeof`, `alignof`, and field `offsetof` values for `Flags`, `Stack`, `Cpu`, and `Sprite` |
| `stack` | Print current `SP`, stack depth, and top bytes in `0x0F00`–`0x0FFF` |
| `regs` | Display `PC`, `A`, `B`, `H`, `SP`, `HEAP`, `Z`, `N`, `C`, and `HALT` |
| `reg <a\|b\|h\|sp\|pc> <val>` | Set register `A`, `B`, `H`, `SP`, or `PC` directly |
| `step` | Execute a single instruction at `PC` |
| `run` | Execute instructions until `HALT` or `MAX_STEPS` (`100000`) |
| `b <addr>` | Run until `PC == addr`, `HALT`, or `MAX_STEPS` |
| `help` | List all available commands |
| `quit` / `exit` | Exit the simulator |

---

## 11. Build, Execution & Verification

```bash
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
./ember.exe
```

To run the automated Lab 07 verification script:
```powershell
Get-Content ..\..\checks\lab-07.txt | .\ember.exe
```