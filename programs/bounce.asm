CLS
LOADI A, 0
LOADI B, 10
move:
    PLOT
    SHOW
    UNPLOT
    INC A
    PUSH B
    LOADI B, 6
    CMP A, B
    POP B
    JNZ move
    HALT
