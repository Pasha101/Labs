# ember — 4KB Computer & CPU Simulator

`ember` is an educational virtual machine and CPU simulator written in C++17 with strict compiler enforcement (`-Wall -Wextra -Werror` and AddressSanitizer/UBSan in Debug builds).

---

## 📦 Part 1: Lab 01 — A Box of Bytes (Memory Infrastructure)

### Overview
Lab 01 establishes the base 4KB flat memory storage and diagnostic tools for inspecting raw byte data.

### Given Infrastructure & Design
- **Memory Box:** Fixed array of 4096 bytes (`using Byte = std::uint8_t; const std::size_t MEM_SIZE = 4096;`).
- **Memory Functions:**
  - `mem_get(mem, addr)`: Reads a byte safely. Returns `0` if address is out of bounds (`addr >= 4096`).
  - `mem_set(mem, addr, val)`: Writes a byte safely. Returns `false` if address is out of bounds.
- **Diagnostics & Inspection:**
  - `dump()`: Hexdump visualization (16 bytes per row) showing Hex offset, Hex bytes, and an ASCII gutter printing printable characters or `.` for non-printable bytes.
  - `show_byte()`: Multi-format inspector showing a single byte in 4 views (Decimal, Hex, Binary, ASCII).

---

## ⚙️ Part 2: Lab 02 — Bits Don't Lie (CPU, ALU & Flags)

### Overview
Lab 02 introduces a virtual 8-bit CPU, general-purpose registers (`A`, `B`), status flags (`Z`, `N`, `C`), and an Arithmetic Logic Unit (ALU).

### Architecture Additions
- **Registers:** `PC` (16-bit Program Counter), `A` and `B` (8-bit registers).
- **Status Flags:** `Z` (Zero), `N` (Negative), `C` (Carry / Borrow).
- **CPU Control:** `HALT` flag, `step` command, `regs` status output.

---

## 🗺️ Part 3: Lab 03 — Addresses, Not Names (Pointers & Memory Access)

### Overview
Lab 03 implements 16-bit Little-Endian memory access, expands the ISA with memory-addressing and I/O instructions, and adds a 16-bit Address Register `H`.

### Memory Architecture & Little-Endian Order
16-bit integers are stored across two contiguous memory bytes:
- **Low Byte (LSB):** Stored at `addr`
- **High Byte (MSB):** Stored at `addr + 1`

For example, storing `0x0800` at `0x0001`:
- `mem[0x0001] = 0x00` (Low byte)
- `mem[0x0002] = 0x08` (High byte)

### Extended Instruction Set Architecture (ISA)

| Opcode | Mnemonic | Description | Flags Updated |
| :---: | :--- | :--- | :---: |
| `0x00` | `NOP` | No operation | None |
| `0x01` | `HALT` | Stop CPU execution | None |
| `0x10`..`0x19` | ALU Ops | `ADD`, `SUB`, `AND`, `OR`, `XOR`, `NOT`, `SHL`, `SHR`, `INC`, `DEC` | Z, N, C |
| `0x20` | `LOADI A, imm8` | Load immediate 8-bit value into A | None |
| `0x21` | `LOAD A, [addr16]` | Load byte from 16-bit address into A | None |
| `0x22` | `STORE [addr16], A` | Store byte from A into 16-bit address | None |
| `0x23` | `LOADH addr16` | Load 16-bit address into register H | None |
| `0x24` | `LOAD A, [H]` | Load byte from address H into A | None |
| `0x25` | `STORE [H], A` | Store byte from A into address H | None |
| `0x26` | `INCH` | Increment 16-bit register H | None |
| `0x27` | `DECH` | Decrement 16-bit register H | None |
| `0x30` | `OUT A` | Print register A as ASCII character | None |
| `0x31` | `OUTN A` | Print register A as decimal integer | None |

---

## 🧪 Worked Trace: Code at 0x000, Data at 0x800

Sample execution loading a character `'H'` (`0x48`) from data section `0x0800` and outputting it via `OUT A`:

```text
ember 0.3 - Addresses, Pointers & Memory (Lab 03). Type `help`.

ember> set 0x800 0x48
ember> set 0 0x21
ember> set 1 0x00
ember> set 2 0x08
ember> set 3 0x30
ember> set 4 0x01

ember> regs
PC: 0000 | A: 0x00 | B: 0x00 | H: 0000 | Z: 0 N: 0 C: 0 | HALT: 0

ember> run
H
ember> regs
PC: 0005 | A: 0x48 | B: 0x00 | H: 0000 | Z: 0 N: 0 C: 0 | HALT: 1