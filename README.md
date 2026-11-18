# ember — 4KB Computer & CPU Simulator

`ember` is an educational virtual machine and 8-bit CPU simulator written in C++17. The project is built with strict compiler diagnostics (`-Wall -Wextra -Werror`) and runtime memory/undefined-behavior sanitizers (`AddressSanitizer` and `UndefinedBehaviorSanitizer` in Debug builds).

---

## Memory Map Overview

`ember` implements a unified 4096-byte (`4 KB`) Von Neumann address space (`0x000` to `0xFFF`) shared between instructions and data:

```text
0x000 ┌─────────────────────────────┐
      │  CODE (2048 bytes)          │  Program instructions (PC starts at 0x000)
0x800 ├─────────────────────────────┤
      │  DATA (512 bytes)           │  Variables, arrays, buffers
0xA00 ├─────────────────────────────┤
      │  VRAM (256 bytes)           │  64 x 32 monochrome display memory
0xB00 ├─────────────────────────────┤
      │  RESERVED / I/O (256 bytes) │  Memory-mapped ports & timer
0xC00 ├─────────────────────────────┤
      │  HEAP (768 bytes)           │  Dynamic allocations (grows upward)
0xF00 ├─────────────────────────────┤
      │  STACK (256 bytes)          │  Call stack (SP starts at 0xFFF, grows downward)
0xFFF └─────────────────────────────┘
```

---

## Part 1: Lab 01 — A Box of Bytes (Memory Infrastructure)

### Overview
Lab 01 establishes the foundational 4KB flat byte-addressable memory model and diagnostic tools for inspecting raw binary data safely.

### Core Design & Implementation
- **Fixed-Width Data Types:** Uses `using Byte = std::uint8_t;` to guarantee an exact unsigned 8-bit cell (`0..255` / `0x00..0xFF`) across all platforms, and `std::size_t` for non-negative memory offsets and sizes (`MEM_SIZE = 4096`).
- **Zero-Initialization:** `Byte data[MEM_SIZE]{};` zero-initializes all 4096 bytes upon construction, preventing Undefined Behavior (UB) from reading uninitialized stack/heap memory.
- **Bounds-Checked Memory API:**
  - `mem_get(mem, addr)`: Returns `mem.data[addr]` if `addr < 4096`; safely returns `0` if `addr >= 4096` without out-of-bounds reads.
  - `mem_set(mem, addr, value)`: Writes `value` to `mem.data[addr]` and returns `true` if `addr < 4096`; leaves memory untouched and returns `false` if out of bounds.
- **Inspection & Diagnostics:**
  - `dump()`: Prints all 4096 bytes in rows of 16 bytes, displaying the 4-digit hex base offset, 16 hex byte values, and an ASCII gutter (printable characters `32..126` shown directly, non-printable bytes shown as `.`).
  - `show_byte()`: Displays a single byte in four formats simultaneously: Decimal, Hexadecimal, 8-bit Binary, and ASCII character.

### Lab 01 Terminal Example
```text
ember> set 0 65
ember> set 1 66
ember> get 0
65  0x41  0b01000001  'A'

ember> dump
0000  41 42 00 00 00 00 00 00 00 00 00 00 00 00 00 00  |AB..............|
```

---

## Part 2: Lab 02 — Bits Don't Lie (CPU, ALU & Flags)

### Overview
Lab 02 introduces the virtual central processing unit (`Cpu`), general-purpose 8-bit registers, processor status flags, and an 8-bit Arithmetic Logic Unit (`ALU`).

### CPU State & Status Flags
- **Registers:**
  - `PC` (16-bit Program Counter): Holds the address of the next instruction to fetch and execute. Starts at `0x0000`.
  - `A`, `B` (8-bit General-Purpose Registers): Primary accumulator (`A`) and secondary operand register (`B`).
- **Status Flags:**
  - `Z` (**Zero Flag**): Set to `1` when the 8-bit operation result equals `0`; otherwise `0`.
  - `N` (**Negative Flag**): Mirrors bit 7 (MSB) of the result (`(result & 0x80) != 0`), indicating a negative number (`-128..-1`) in 8-bit Two's Complement representation.
  - `C` (**Carry / Borrow Flag**): Set to `1` on unsigned 8-bit addition overflow (`sum > 255`), subtraction borrow (`A < B`), or the bit shifted out during `SHL`/`SHR`. Always cleared (`0`) on bitwise logic operations.
- **Execution State:** `halted` boolean flag stops execution when `HALT` (`0x00`) is encountered.
- **Interactive Commands:** `regs` (display registers and flags), `step` (execute one instruction), `reg <a|b> <val>` (manually set register values).

### Control & ALU Instruction Set (`0x00`–`0x19`)

| Opcode | Mnemonic | Size | Operation | Flags Updated |
| :---: | :--- | :---: | :--- | :---: |
| `0x00` | `HALT` | 1 | Stop CPU execution (`halted = true`) | None |
| `0x01` | `NOP` | 1 | No operation | None |
| `0x10` | `ADD A, B` | 1 | `A = (A + B) & 0xFF` | Z, N, C |
| `0x11` | `SUB A, B` | 1 | `A = (A - B) & 0xFF` | Z, N, C |
| `0x12` | `AND A, B` | 1 | `A = A & B` | Z, N, C=0 |
| `0x13` | `OR A, B` | 1 | `A = A \| B` | Z, N, C=0 |
| `0x14` | `XOR A, B` | 1 | `A = A ^ B` | Z, N, C=0 |
| `0x15` | `NOT A` | 1 | `A = ~A` | Z, N, C=0 |
| `0x16` | `SHL A` | 1 | Logical shift left (`A = A << 1`) | Z, N, C |
| `0x17` | `SHR A` | 1 | Logical shift right (`A = A >> 1`) | Z, N, C |
| `0x18` | `INC A` | 1 | `A = (A + 1) & 0xFF` | Z, N |
| `0x19` | `DEC A` | 1 | `A = (A - 1) & 0xFF` | Z, N |

### Worked ALU Examples & Session
1. **Unsigned Overflow (`200 + 100`):** `0xC8 + 0x64 = 300 (0x012C)`. Truncated to 8 bits: `A = 44 (0x2C)`, `Z = 0`, `N = 0`, **`C = 1`** (carry out).
2. **Negative Two's Complement Result (`10 - 20`):** `0x0A - 0x14 = -10`, represented in 8-bit unsigned as `246 (0xF6 = 0b11110110)`. Flags: `Z = 0`, **`N = 1`** (bit 7 is `1`), **`C = 1`** (borrow occurred since `10 < 20`).

```text
ember> reg a 200
ember> reg b 100
ember> set 0 0x10
ember> step
ember> regs
PC: 0001 | A: 0x2c | B: 0x64 | H: 0x0000 | Z: 0 N: 0 C: 1 | HALT: 0
```

---

## Part 3: Lab 03 — Addresses, Not Names (Pointers & Memory Access)

### Overview
Lab 03 enables the CPU to read and write arbitrary memory locations, introduces 16-bit Little-Endian encoding, adds the 16-bit address register `H` for pointer-like indirect addressing, and provides character/numeric I/O instructions.

### Little-Endian 16-Bit Memory Access (`get16` / `set16`)
Because memory cells are 8-bit bytes while addresses span 12 bits (`0x000`..`0xFFF`), 16-bit addresses and values are stored across two consecutive bytes in **Little-Endian** order:
- **Low Byte (`value & 0xFF`):** Stored at `addr`.
- **High Byte (`(value >> 8) & 0xFF`):** Stored at `addr + 1`.
- Both `get16` and `set16` check `addr + 1 >= MEM_SIZE` so reading or writing at the final byte `0xFFF` never overflows the array.

### Data Movement & I/O Instruction Set (`0x02`–`0x03`, `0x20`–`0x2D`)

| Opcode | Mnemonic | Size | Operation | Flags Updated |
| :---: | :--- | :---: | :--- | :---: |
| `0x02` | `OUT` | 1 | Print `A` as an ASCII character | None |
| `0x03` | `OUTN` | 1 | Print `A` as a decimal integer followed by a space | None |
| `0x20` | `LOADI A, imm8` | 2 | Load immediate 8-bit constant into `A` | None |
| `0x21` | `LOADI B, imm8` | 2 | Load immediate 8-bit constant into `B` | None |
| `0x22` | `LOAD A, [addr16]` | 3 | Direct load: `A = mem[addr16]` | None |
| `0x23` | `LOAD B, [addr16]` | 3 | Direct load: `B = mem[addr16]` | None |
| `0x24` | `STORE [addr16], A` | 3 | Direct store: `mem[addr16] = A` | None |
| `0x25` | `STORE [addr16], B` | 3 | Direct store: `mem[addr16] = B` | None |
| `0x26` | `MOV A, B` | 1 | Copy register: `A = B` | None |
| `0x27` | `MOV B, A` | 1 | Copy register: `B = A` | None |
| `0x28` | `LOADH H, imm16` | 3 | Load 16-bit address into address register `H` | None |
| `0x29` | `LOAD A, [H]` | 1 | Indirect load (dereference): `A = mem[H]` | None |
| `0x2A` | `STORE [H], A` | 1 | Indirect store: `mem[H] = A` | None |
| `0x2B` | `INCH` | 1 | Increment address register: `H = H + 1` | None |
| `0x2C` | `DECH` | 1 | Decrement address register: `H = H - 1` | None |
| `0x2D` | `HLOW` | 1 | Copy low byte of `H` to `A`: `A = H & 0xFF` | None |

*(Note: `LOAD` and `LOADI` do not modify `Z`, `N`, or `C`. To branch on a loaded value, an explicit `CMP`, `INC`, or `DEC` instruction must follow.)*

### Worked Trace: Code at `0x000`, Data at `0x800`
Storing `'H'` (`0x48`) in the data segment (`0x0800`) and loading/printing it from code (`0x0000`):

```text
addr  bytes      instruction
0000  22 00 08   LOAD A, [0x0800]  ; little-endian operand 00 08 -> A = mem[0x0800] ('H')
0003  02         OUT               ; print 'H'
0004  00         HALT              ; stop execution
```

### Theory: Host Pointer vs Guest Address
- **Guest Address (`std::size_t addr` in `ember`):** An integer index (`0` to `4095`) relative to the start of the emulated `Memory::data` array. It is deterministic, starts at `0x0000` on every run, and is completely sandboxed inside the virtual machine.
- **Host Pointer (`Byte*` in C++):** A 64-bit virtual memory address managed by the host OS and MMU (e.g., `&mem.data[0x0800]`). Host pointers are subject to Address Space Layout Randomization (ASLR), differ across runs, and are guarded by AddressSanitizer redzones in Debug builds.

---

## Part 4: Lab 04 — The Shape of Control (Branches, Loops & Scope)

### Overview
Lab 04 transforms `ember` from a linear calculator into a Turing-complete programmable machine by adding comparison, unconditional/conditional jumps, bytecode loops, infinite-loop protection, and linear search in both host C++ and guest machine code.

### Control Flow, Safety & C++ Mechanics
- **Complete `switch (opcode)` Decoder (`step`):** Every valid instruction has an explicit `case` terminated with `break;` (preventing accidental `switch` fall-through). Any unrecognized byte hits `default:`, prints `unknown opcode 0x..`, sets `cpu.halted = true`, and returns `false`.
- **Comparison & Branch Instructions (`0x1A`, `0x30`–`0x34`):**
  - When a branch is **taken**, `PC` is set directly to `addr16` **without** adding the 3-byte instruction size. When **not taken**, `PC += 3`.
- **Infinite Loop Protection & Breakpoints (`run` / `b`):**
  - `run` executes instructions in a loop up to `MAX_STEPS = 100000`. If a broken `JMP` creates an infinite loop, execution stops safely and prints `step limit (100000) reached`.
  - `b <addr>` runs until `PC == addr` (or `HALT` / step limit) for debugging loops.
- **Host Linear Search (`find <lo> <hi> <byte>`):** Implemented in C++ using a bounded `for` loop over `[lo, hi)`, returning the first matching address or `not found` if `i` reaches `hi`.
- **C++ Scope & Lifetime:** Block scopes `{ }` isolate local variables and allow inner declarations to shadow outer identifiers, while local `static` variables persist across calls without exposing their names outside their enclosing block.

### Comparison & Branch Instruction Set (`0x1A`, `0x30`–`0x34`)

| Opcode | Mnemonic | Size | Operation | Flags Updated |
| :---: | :--- | :---: | :--- | :---: |
| `0x1A` | `CMP A, B` | 1 | Compute `A - B`, update flags, discard result | Z, N, C |
| `0x30` | `JMP addr16` | 3 | Unconditional jump: `PC = addr16` | None |
| `0x31` | `JZ addr16` | 3 | Jump if Zero (`Z == 1`): `PC = addr16`, else `PC += 3` | None |
| `0x32` | `JNZ addr16` | 3 | Jump if Not Zero (`Z == 0`): `PC = addr16`, else `PC += 3` | None |
| `0x33` | `JC addr16` | 3 | Jump if Carry (`C == 1`): `PC = addr16`, else `PC += 3` | None |
| `0x34` | `JNC addr16` | 3 | Jump if No Carry (`C == 0`): `PC = addr16`, else `PC += 3` | None |

---

### Listing 1: Countdown `3 2 1` (Guest Bytecode vs C++ `while`)

#### Machine Code Listing (`0x0000`..`0x0007`)
```text
addr  bytes      instruction
0000  20 03      LOADI A, 3        ; A = 3
0002  03         loop: OUTN        ; print A and space
0003  19               DEC A       ; A = A - 1 (sets Z when A == 0)
0004  32 02 00         JNZ loop    ; if Z == 0, jump to 0x0002
0007  00               HALT        ; stop execution
```
**Output:** `3 2 1 `

#### Execution Trace Side-by-Side with C++ `while`

| Step | `PC` (before) | Instruction | `A` (after) | `Z` (after) | Equivalent C++ `while (a > 0)` |
| :---: | :---: | :--- | :---: | :---: | :--- |
| 1 | `0x0000` | `LOADI A, 3` | `3` | `0` | `int a = 3;` |
| 2 | `0x0002` | `OUTN` | `3` | `0` | `std::cout << a << ' ';` (prints `3 `) |
| 3 | `0x0003` | `DEC A` | `2` | `0` | `--a;` (`a` becomes `2`) |
| 4 | `0x0004` | `JNZ 0x0002` | `2` | `0` | `while (a > 0)` -> `true`, repeat |
| 5 | `0x0002` | `OUTN` | `2` | `0` | `std::cout << a << ' ';` (prints `2 `) |
| 6 | `0x0003` | `DEC A` | `1` | `0` | `--a;` (`a` becomes `1`) |
| 7 | `0x0004` | `JNZ 0x0002` | `1` | `0` | `while (a > 0)` -> `true`, repeat |
| 8 | `0x0002` | `OUTN` | `1` | `0` | `std::cout << a << ' ';` (prints `1 `) |
| 9 | `0x0003` | `DEC A` | `0` | `1` | `--a;` (`a` becomes `0`, `Z = 1`) |
| 10 | `0x0004` | `JNZ 0x0002` | `0` | `1` | `while (a > 0)` -> `false`, exit loop |
| 11 | `0x0007` | `HALT` | `0` | `1` | end of loop |

---

### Listing 2: Guest Linear Search in `[0x0800, 0x0808)`

Searches for byte `0x41` (`'A'`) across an 8-byte buffer starting at `0x0800`:
- **Loop Termination at `hi` (`0x0808`):** After incrementing `H` with `INCH`, `HLOW` loads the low byte of `H` (`0x00`..`0x08`) into `A` and compares it with `0x08` using `CMP A, B`. When `H` reaches `0x0808`, `Z` becomes `1` and `JNZ loop` falls through to `miss`.
- **Match vs Miss Reporting:** When `mem[H] == 0x41`, `JZ found` jumps to `0x0016`, where `HLOW` and `OUTN` print the matching offset (`0`..`7`). If the entire range is scanned without a match, execution falls through to `miss` (`0x0012`), which loads `255` (`0xFF`) into `A` and prints `255` via `OUTN`.

```text
addr  bytes      instruction
0000  28 00 08   LOADH H, 0x0800     ; H = 0x0800 (start of 8-byte buffer)
0003  29         loop:  LOAD A, [H]  ; A = mem[H]
0004  21 41             LOADI B, 0x41; B = 0x41 (target byte 'A')
0006  1A                CMP A, B     ; compare mem[H] with 0x41
0007  31 16 00          JZ found     ; if equal (Z == 1), jump to 0x0016
000A  2B                INCH         ; H = H + 1
000B  2D                HLOW         ; A = H & 0xFF (current offset 0..8)
000C  21 08             LOADI B, 0x08; B = 8 (end offset for 0x0808)
000E  1A                CMP A, B     ; reached 0x0808?
000F  32 03 00          JNZ loop     ; if not (Z == 0), continue loop
0012  20 FF      miss:  LOADI A, 255 ; miss indicator (0xFF)
0014  03                OUTN         ; print 255
0015  00                HALT         ; stop
0016  2D         found: HLOW         ; A = H & 0xFF (matching offset 0..7)
0017  03                OUTN         ; print matching offset
0018  00                HALT         ; stop
```

---

## Build & Execution

```bash
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
./ember.exe
```