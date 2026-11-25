; search.asm - Readable linear search in [0x0800, 0x0808) for 'A' (0x41)
LOADI A, 0x58
STORE [0x0800], A
STORE [0x0801], A
LOADI A, 0x41
STORE [0x0802], A

LOADH H, 0x0800
loop:
    LOAD A, [H]
    LOADI B, 0x41
    CMP A, B
    JZ found
    INCH
    HLOW
    LOADI B, 0x08
    CMP A, B
    JNZ loop
miss:
    LOADI A, 255
    OUTN
    HALT
found:
    HLOW
    OUTN
    HALT
