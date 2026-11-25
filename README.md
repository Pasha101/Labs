# ember `v1.0.0` — 4KB Virtual Computer, 8-Bit CPU & Two-Pass Assembler

`ember` is a complete educational 8-bit virtual machine and assembly toolchain written in C++17 (`-Wall -Wextra -Werror`). It features a flat 4096-byte Von Neumann RAM, a 45-instruction ISA, memory-mapped I/O, a 64×32 monochrome framebuffer, a guest bump-allocator heap, an empty-descending call stack supporting recursion, a lexer with line-numbered syntax diagnostics, a linked-list two-pass macro-free assembler, and a disassembler.

---

## Table of Contents
1. [System Pipeline & Architecture](#1-system-pipeline--architecture)
2. [Complete Memory Map (`0x000`–`0xFFF`)](#2-complete-memory-map-0x0000xfff)
3. [Quick Build & Run (3 Commands)](#3-quick-build--run-3-commands)
4. [Complete Instruction Set Architecture (ISA)](#4-complete-instruction-set-architecture-isa)
5. [Calling Convention & Stack Discipline](#5-calling-convention--stack-discipline)
6. [Assembly Language, Lexer & Two-Pass Assembler](#6-assembly-language-lexer--two-pass-assembler)
7. [Verified Terminal Outputs](#7-verified-terminal-outputs)
8. [Interactive Console Commands (REPL)](#8-interactive-console-commands-repl)
9. [Honest Limitations](#9-honest-limitations)
10. [Retrospective: Labs 01–08](#10-retrospective-labs-0108)

---

## 1. System Pipeline & Architecture

```text
┌──────────────┐    ┌───────────────┐    ┌─────────────────────┐    ┌──────────────────┐    ┌────────────────┐
│  .asm file   │───▶│  Lexer        │───▶│  Two-Pass Assembler │───▶│  4KB RAM & CPU   │───▶│  64x32 Screen  │
│  (UTF-8 text)│    │  (Token list) │    │  (Label linked list)│    │  (step / run)    │    │  & Console I/O │
└──────────────┘    └───────────────┘    └─────────────────────┘    └──────────────────┘    └────────────────┘
```

### Module Structure (`CMakeLists.txt`)
- `src/memory.hpp` / `src/memory.cpp`: 4096-byte RAM (`Byte = std::uint8_t`), bounds-checked byte access (`mem_get`, `mem_set`), and Little-Endian 16-bit helpers (`get16`, `set16`).
- `src/dump.hpp` / `src/dump.cpp`: 16-byte-per-row `dump()` hexdump with ASCII sidebar and 4-format `show_byte()` inspector.
- `src/alu.hpp` / `src/alu.cpp`: 8-bit Arithmetic Logic Unit (`ADD`, `SUB`, `MUL`, bitwise/shifts) and `Z`/`N`/`C` flag updates.
- `src/stack.hpp` / `src/stack.cpp`: Encapsulated `Stack` ADT (`0x0F00`–`0x0FFF`) with overflow/underflow protection.
- `src/cpu.hpp` / `src/cpu.cpp`: Strongly typed `enum class Op : Byte`, `struct Cpu`, `step()` decoder, guest bump-allocator (`Op::Alloc`), `CALL`/`RET`/`PUSH`/`POP`, and MMIO port hooks.
- `src/screen.hpp` / `src/screen.cpp`: 64×32 monochrome framebuffer (`show`, `plot`, `unplot`, `clear`, `draw_frame`) mapped to `0x0A00`–`0x0AFF`.
- `src/asm/token.hpp` / `src/asm/lexer.hpp` / `src/asm/lexer.cpp`: Hand-written scanner supporting decimal, `0x` hex, and `0b` binary literals, UTF-8 BOM skipping, line-numbered error reporting, and linked-list memory cleanup (`destroy`).
- `src/asm/assembler.hpp` / `src/asm/assembler.cpp`: Two-pass assembler with linked-list symbol table (`LabelNode`) and `disassemble_range()` byte-to-listing disassembler.
- `src/main.cpp`: CLI file runner (`./ember program.asm`), interactive REPL debugger, `struct Sprite` engine (`Sprite sprites[8]`), and struct memory layout inspector (`layout`).

---

## 2. Complete Memory Map (`0x000`–`0xFFF`)

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

### Framebuffer Addressing (`0x0A00`–`0x0AFF`)
Each row of 64 pixels occupies 8 bytes (MSB = leftmost pixel):
```text
byte_addr = 0x0A00 + y * 8 + (x / 8)
bit_index = 7 - (x % 8)
```

---

## 3. Quick Build & Run (3 Commands)

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
./build/ember.exe programs/fib.asm
```

*(Running `./build/ember.exe` with no arguments launches the interactive REPL debugger.)*

---

## 4. Complete Instruction Set Architecture (ISA)

| Opcode | Enum (`Op`) | Mnemonic | Size | Flags | Operation |
| :---: | :--- | :--- | :---: | :---: | :--- |
| `0x00` | `Op::Halt` | `HALT` | 1 | — | Stop CPU execution (`cpu.halted = true`) |
| `0x01` | `Op::Nop` | `NOP` | 1 | — | No operation |
| `0x02` | `Op::Out` | `OUT` | 1 | — | Print `A` as ASCII character |
| `0x03` | `Op::Outn` | `OUTN` | 1 | — | Print `A` as decimal integer + space |
| `0x04` | `Op::Outs` | `OUTS addr16` | 3 | — | Print C-string at `addr16` until `'\0'` (max 256 bytes) |
| `0x10` | `Op::Add` | `ADD A, B` | 1 | Z N C | `A = A + B` |
| `0x11` | `Op::Sub` | `SUB A, B` | 1 | Z N C | `A = A - B` |
| `0x12` | `Op::And` | `AND A, B` | 1 | Z N, C=0 | `A = A & B` |
| `0x13` | `Op::Or` | `OR A, B` | 1 | Z N, C=0 | `A = A \| B` |
| `0x14` | `Op::Xor` | `XOR A, B` | 1 | Z N, C=0 | `A = A ^ B` |
| `0x15` | `Op::Not` | `NOT A` | 1 | Z N, C=0 | `A = ~A` |
| `0x16` | `Op::Shl` | `SHL A` | 1 | Z N C | `A = A << 1` (`C` = old bit 7) |
| `0x17` | `Op::Shr` | `SHR A` | 1 | Z N C | `A = A >> 1` (`C` = old bit 0) |
| `0x18` | `Op::Inc` | `INC A` | 1 | Z N | `A = A + 1` |
| `0x19` | `Op::Dec` | `DEC A` | 1 | Z N | `A = A - 1` |
| `0x1A` | `Op::Cmp` | `CMP A, B` | 1 | Z N C | Compute `A - B` for flags, discard result |
| `0x1B` | `Op::Mul` | `MUL A, B` | 1 | Z N C | `A = (A * B) & 0xFF`; `C = 1` if `A * B > 255` |
| `0x20` | `Op::LoadiA` | `LOADI A, imm8` | 2 | — | `A = imm8` |
| `0x21` | `Op::LoadiB` | `LOADI B, imm8` | 2 | — | `B = imm8` |
| `0x22` | `Op::LoadA` | `LOAD A, [addr16]` | 3 | — | `A = mem[addr16]` (reads `TICKS` at `0x0B01`) |
| `0x23` | `Op::LoadB` | `LOAD B, [addr16]` | 3 | — | `B = mem[addr16]` |
| `0x24` | `Op::StoreA` | `STORE [addr16], A` | 3 | — | `mem[addr16] = A` (prints char if `0x0B00`) |
| `0x25` | `Op::StoreB` | `STORE [addr16], B` | 3 | — | `mem[addr16] = B` |
| `0x26` | `Op::MovAB` | `MOV A, B` | 1 | — | `A = B` |
| `0x27` | `Op::MovBA` | `MOV B, A` | 1 | — | `B = A` |
| `0x28` | `Op::LoadH` | `LOADH H, imm16` | 3 | — | `H = imm16` |
| `0x29` | `Op::LoadAH` | `LOAD A, [H]` | 1 | — | `A = mem[H]` |
| `0x2A` | `Op::StoreHA` | `STORE [H], A` | 1 | — | `mem[H] = A` |
| `0x2B` | `Op::IncH` | `INCH` | 1 | — | `H = H + 1` |
| `0x2C` | `Op::DecH` | `DECH` | 1 | — | `H = H - 1` |
| `0x2D` | `Op::HLow` | `HLOW` | 1 | — | `A = H & 0xFF` |
| `0x30` | `Op::Jmp` | `JMP addr16` | 3 | — | `PC = addr16` |
| `0x31` | `Op::Jz` | `JZ addr16` | 3 | — | If `Z == 1`, `PC = addr16`; else `PC += 3` |
| `0x32` | `Op::Jnz` | `JNZ addr16` | 3 | — | If `Z == 0`, `PC = addr16`; else `PC += 3` |
| `0x33` | `Op::Jc` | `JC addr16` | 3 | — | If `C == 1`, `PC = addr16`; else `PC += 3` |
| `0x34` | `Op::Jnc` | `JNC addr16` | 3 | — | If `C == 0`, `PC = addr16`; else `PC += 3` |
| `0x40` | `Op::Cls` | `CLS` | 1 | — | Clear 256 bytes of VRAM (`0x0A00`–`0x0AFF`) |
| `0x41` | `Op::Plot` | `PLOT` | 1 | C | Set pixel `(x=A, y=B)`; sets `C=1` if out of bounds |
| `0x42` | `Op::Unplot` | `UNPLOT` | 1 | C | Clear pixel `(x=A, y=B)`; sets `C=1` if out of bounds |
| `0x43` | `Op::Show` | `SHOW` | 1 | — | Render 64×32 VRAM to terminal |
| `0x50` | `Op::PushA` | `PUSH A` | 1 | — | Push `A` onto stack (`mem[SP--] = A`) |
| `0x51` | `Op::PushB` | `PUSH B` | 1 | — | Push `B` onto stack (`mem[SP--] = B`) |
| `0x52` | `Op::PopA` | `POP A` | 1 | — | Pop top of stack into `A` (`A = mem[++SP]`) |
| `0x53` | `Op::PopB` | `POP B` | 1 | — | Pop top of stack into `B` (`B = mem[++SP]`) |
| `0x54` | `Op::Call` | `CALL addr16` | 3 | — | Push low then high byte of `PC + 3`, jump to `addr16` |
| `0x55` | `Op::Ret` | `RET` | 1 | — | Pop high then low byte into `PC` |
| `0x56` | `Op::PushH` | `PUSH H` | 1 | — | Push low then high byte of `H` onto stack |
| `0x57` | `Op::PopH` | `POP H` | 1 | — | Pop high then low byte from stack into `H` |
| `0x60` | `Op::Alloc` | `ALLOC` | 1 | C | Allocate `A` bytes in heap (`0x0C00`–`0x0EFF`) into `H`; `C=1` if full |

---

## 5. Calling Convention & Stack Discipline

- **Empty Descending Stack (`0x0F00`–`0x0FFF`):** `SP` starts at `0x0FFF` (next free slot) and decrements on `PUSH`. Crossing below `0x0F00` triggers `stack overflow`; popping above `0x0FFF` triggers `stack underflow`. Both halt the CPU cleanly.
- **Arguments & Return Values:** A function receives its 8-bit argument in **`A`** and returns its 8-bit result in **`A`**.
- **Caller-Saved Registers:** **`B`** and **`H`** (as well as `A` across nested calls) are caller-saved. Any caller that needs `A`, `B`, or `H` preserved across `CALL` must `PUSH` them before `CALL` and `POP` them after `RET`.
- **Return Address Byte Order:** `CALL addr16` pushes `(PC + 3) & 0xFF` (low byte) first, then `(PC + 3) >> 8` (high byte). `RET` pops high byte first, then low byte.

---

## 6. Assembly Language, Lexer & Two-Pass Assembler

### Why Two Passes Are Needed
Forward jumps such as `JZ found` or `CALL fib` reference labels defined later in the file whose byte offsets are not yet known during Pass 1:
- **Pass 1:** Walks the linked list of `Token` nodes (`Token* head`), records every `label:` and its exact byte offset in a linked list (`LabelNode*`), and advances `pc` using the exact instruction sizes from the ISA.
- **Pass 2:** Emits opcodes and operands into `Memory`, resolving all label references into 16-bit Little-Endian addresses. Both linked lists are freed via `delete` before returning.

### Before vs After: `search.asm` (Lab 08) vs Hand-Assembled Hex (Lab 04)

| Lab 08 Readable Assembly (`programs/search.asm`) | Lab 04 Manual Hex Bytecode |
| :--- | :--- |
| ```asm<br>LOADH H, 0x0800<br>loop:<br>    LOAD A, [H]<br>    LOADI B, 0x41<br>    CMP A, B<br>    JZ found<br>    INCH<br>    HLOW<br>    LOADI B, 0x08<br>    CMP A, B<br>    JNZ loop<br>miss:<br>    LOADI A, 255<br>    OUTN<br>    HALT<br>found:<br>    HLOW<br>    OUTN<br>    HALT<br>``` | ```text<br>0000: 28 00 08<br>0003: 29<br>0004: 21 41<br>0006: 1A<br>0007: 31 16 00<br>000A: 2B<br>000B: 2D<br>000C: 21 08<br>000E: 1A<br>000F: 32 03 00<br>0012: 20 FF<br>0014: 03<br>0015: 00<br>0016: 2D<br>0017: 03<br>0018: 00<br>``` |

Inserting a single instruction in Lab 04 required manually recalculating jump offsets (`0x0016`, `0x0003`); with labels in Lab 08, addresses are resolved automatically.

---

## 7. Verified Terminal Outputs

### 7.1. Program Execution (`hello.asm`, `search.asm`, `fib.asm`)
```text
$ ./build/ember.exe programs/hello.asm
HELLO

$ ./build/ember.exe programs/search.asm
2 

$ ./build/ember.exe programs/fib.asm
8 
```
*(Changing `LOADI A, 6` to `LOADI A, 10` in `programs/fib.asm` outputs `55 `.)*

### 7.2. Lexer Output (`lex programs/hello.asm`)
```text
ember> lex programs/hello.asm
line 1: Newline (\n)
line 2: Ident (LOADI)
line 2: Ident (A)
line 2: Comma (,)
line 2: Number (0x48)
line 2: Newline (\n)
line 3: Ident (STORE)
line 3: LBracket ([)
line 3: Number (0x0800)
line 3: RBracket (])
line 3: Comma (,)
line 3: Ident (A)
line 3: Newline (\n)
...
line 14: Ident (OUTS)
line 14: Number (0x0800)
line 14: Newline (\n)
line 15: Ident (HALT)
line 15: Eof ()
```
Invalid literals such as `0xGG`, `0b102`, or `12ab` are rejected immediately with exact line numbers (e.g., `line 4: bad hex number`).

### 7.3. Memory `dump` of Assembled `programs/fib.asm` (36 Bytes)
```text
0000: 20 06 54 07 00 03 00 21  00 1a 31 23 00 21 01 1a  | .T....!..1#.!..|
0010: 31 23 00 50 19 54 07 00  53 50 26 19 19 54 07 00  |1#.P.T..SP&..T..|
0020: 53 10 55 55 00 00 00 00  00 00 00 00 00 00 00 00  |S.UU............|
```

### 7.4. Framebuffer `show` Output (`programs/bounce.asm`, Frame 1)
```text
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
#...............................................................
................................................................
```

---

## 8. Interactive Console Commands (REPL)

| Command | Description |
| :--- | :--- |
| `lex <file.asm>` | Tokenize an assembly file and print each token with its line number |
| `asm <file.asm>` | Assemble an assembly file into `0x0000` and reset CPU state |
| `disasm <lo> <hi>` | Disassemble machine bytes in `[lo, hi)` back into readable instructions |
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
| `tick [n]` | Advance active sprites `n` steps with boundary reflection and render |
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

## 9. Honest Limitations

- **One Instruction Per Line:** The grammar requires a newline (`\n`) after each instruction; multiple instructions on a single line are rejected.
- **No Constant Expressions or Macros:** Operands must be single numbers, registers, or labels (expressions like `0x0800 + 4` or `.define` macros are not supported).
- **No String Literals in `DB`:** The `DB` directive accepts comma-separated 8-bit numeric literals (`DB 0x48, 0x49, 0x00`) rather than quoted `"..."` strings.
- **8-Bit Arithmetic Range:** `fib(n)` fits in an unsigned byte up to `fib(13) = 233`; `fib(14) = 377` wraps modulo 256 with `C = 1`.

---

## 10. Retrospective: Labs 01–08

What stood out most across building `ember` from scratch is how every "high-level" programming construct reduces to explicit byte movement and strict contracts. In Lab 05, seeing that a 64×32 screen is nothing more than 256 bytes of RAM where `0xFF` lights eight pixels in a row demystified graphics hardware completely. In Lab 07, tracing recursive `fact(5)` showed why calling conventions exist: without an explicit agreement on who saves `B` and `H` on the stack, recursion immediately corrupts caller state. Finally, in Lab 08, writing a two-pass assembler proved why symbolic languages were invented—hand-patching jump offsets in Lab 04 made inserting a single instruction error-prone, whereas adding a lexer and a linked-list symbol table turned the same 4KB machine into a platform where recursive algorithms like `fib.asm` can be written and modified effortlessly.