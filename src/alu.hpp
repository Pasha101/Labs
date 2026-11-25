#pragma once

#include <cstdint>
#include "memory.hpp"

struct Flags {
    bool z{false};
    bool n{false};
    bool c{false};
};

void update_zn_flags(Flags& flags, Byte result);

Byte alu_add(Byte a, Byte b, Flags& flags);
Byte alu_sub(Byte a, Byte b, Flags& flags);
Byte alu_and(Byte a, Byte b, Flags& flags);
Byte alu_or(Byte a, Byte b, Flags& flags);
Byte alu_xor(Byte a, Byte b, Flags& flags);
Byte alu_not(Byte a, Flags& flags);
Byte alu_shl(Byte a, Flags& flags);
Byte alu_shr(Byte a, Flags& flags);
Byte alu_inc(Byte a, Flags& flags);
Byte alu_dec(Byte a, Flags& flags);
Byte alu_mul(Byte a, Byte b, Flags& flags);