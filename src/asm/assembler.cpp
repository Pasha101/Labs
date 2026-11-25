#include "assembler.hpp"
#include "lexer.hpp"
#include "../cpu.hpp"

#include <cctype>
#include <iostream>
#include <iomanip>

struct LabelNode {
    std::string name;
    std::uint16_t addr;
    LabelNode* next;
};

static void destroy_labels(LabelNode* head) {
    while (head) {
        LabelNode* next = head->next;
        delete head;
        head = next;
    }
}

static bool add_label(LabelNode*& head, const std::string& name, std::uint16_t addr) {
    for (LabelNode* cur = head; cur; cur = cur->next) {
        if (cur->name == name) {
            return false;
        }
    }
    head = new LabelNode{name, addr, head};
    return true;
}

static bool find_label(const LabelNode* head, const std::string& name, std::uint16_t& out) {
    for (const LabelNode* cur = head; cur; cur = cur->next) {
        if (cur->name == name) {
            out = cur->addr;
            return true;
        }
    }
    return false;
}

static std::string to_upper(const std::string& s) {
    std::string res = s;
    for (char& ch : res) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return res;
}

static bool parse_number_token(const std::string& text, long& out) {
    try {
        if (text.size() > 2 && text[0] == '0' && (text[1] == 'b' || text[1] == 'B')) {
            out = std::stol(text.substr(2), nullptr, 2);
            return true;
        }
        std::size_t used = 0;
        out = std::stol(text, &used, 0);
        return used == text.size();
    } catch (...) {
        return false;
    }
}

static std::string line_err(int line, const std::string& msg) {
    return "line " + std::to_string(line) + ": " + msg;
}

static bool expect_end_of_line(Token*& cur, int line, std::string& err) {
    if (cur->kind == Token::Kind::Newline) {
        cur = cur->next;
        return true;
    }
    if (cur->kind == Token::Kind::Eof) {
        return true;
    }
    err = line_err(line, "unexpected token '" + cur->text + "'");
    return false;
}

static bool resolve_u16(const Token* tok, const LabelNode* labels, bool pass2, std::uint16_t& out, std::string& err) {
    if (tok->kind == Token::Kind::Number) {
        long val = 0;
        if (!parse_number_token(tok->text, val) || val < 0 || val > 0xFFFF) {
            err = line_err(tok->line, "16-bit value out of range: " + tok->text);
            return false;
        }
        out = static_cast<std::uint16_t>(val);
        return true;
    }
    if (tok->kind == Token::Kind::Ident) {
        if (!pass2) {
            out = 0;
            return true;
        }
        if (!find_label(labels, tok->text, out)) {
            err = line_err(tok->line, "undefined label '" + tok->text + "'");
            return false;
        }
        return true;
    }
    err = line_err(tok->line, "expected address or label, got '" + tok->text + "'");
    return false;
}

static bool resolve_u8(const Token* tok, Byte& out, std::string& err) {
    if (tok->kind != Token::Kind::Number) {
        err = line_err(tok->line, "expected 8-bit number, got '" + tok->text + "'");
        return false;
    }
    long val = 0;
    if (!parse_number_token(tok->text, val) || val < 0 || val > 255) {
        err = line_err(tok->line, "8-bit value out of range (0..255): " + tok->text);
        return false;
    }
    out = static_cast<Byte>(val);
    return true;
}

static bool encode_instruction(Token*& cur,
                               std::uint16_t& pc,
                               Memory& mem,
                               const LabelNode* labels,
                               bool pass2,
                               std::string& err) {
    int line = cur->line;
    std::string mnem = to_upper(cur->text);
    cur = cur->next;

    auto emit1 = [&](Op op) -> bool {
        if (pc >= MEM_SIZE) {
            err = line_err(line, "program exceeds 4096 bytes");
            return false;
        }
        if (pass2) {
            mem_set(mem, pc, static_cast<Byte>(op));
        }
        pc = static_cast<std::uint16_t>(pc + 1);
        return true;
    };

    auto emit2 = [&](Op op, Byte imm) -> bool {
        if (static_cast<std::size_t>(pc) + 1 >= MEM_SIZE) {
            err = line_err(line, "program exceeds 4096 bytes");
            return false;
        }
        if (pass2) {
            mem_set(mem, pc, static_cast<Byte>(op));
            mem_set(mem, pc + 1, imm);
        }
        pc = static_cast<std::uint16_t>(pc + 2);
        return true;
    };

    auto emit3 = [&](Op op, std::uint16_t addr) -> bool {
        if (static_cast<std::size_t>(pc) + 2 >= MEM_SIZE) {
            err = line_err(line, "program exceeds 4096 bytes");
            return false;
        }
        if (pass2) {
            mem_set(mem, pc, static_cast<Byte>(op));
            set16(mem, pc + 1, addr);
        }
        pc = static_cast<std::uint16_t>(pc + 3);
        return true;
    };

    if (mnem == "HALT")   return emit1(Op::Halt)   && expect_end_of_line(cur, line, err);
    if (mnem == "NOP")    return emit1(Op::Nop)    && expect_end_of_line(cur, line, err);
    if (mnem == "OUT")    return emit1(Op::Out)    && expect_end_of_line(cur, line, err);
    if (mnem == "OUTN")   return emit1(Op::Outn)   && expect_end_of_line(cur, line, err);
    if (mnem == "INCH")   return emit1(Op::IncH)   && expect_end_of_line(cur, line, err);
    if (mnem == "DECH")   return emit1(Op::DecH)   && expect_end_of_line(cur, line, err);
    if (mnem == "HLOW")   return emit1(Op::HLow)   && expect_end_of_line(cur, line, err);
    if (mnem == "CLS")    return emit1(Op::Cls)    && expect_end_of_line(cur, line, err);
    if (mnem == "PLOT")   return emit1(Op::Plot)   && expect_end_of_line(cur, line, err);
    if (mnem == "UNPLOT") return emit1(Op::Unplot) && expect_end_of_line(cur, line, err);
    if (mnem == "SHOW")   return emit1(Op::Show)   && expect_end_of_line(cur, line, err);
    if (mnem == "RET")    return emit1(Op::Ret)    && expect_end_of_line(cur, line, err);
    if (mnem == "ALLOC")  return emit1(Op::Alloc)  && expect_end_of_line(cur, line, err);

    if (mnem == "NOT" || mnem == "SHL" || mnem == "SHR" || mnem == "INC" || mnem == "DEC") {
        if (cur->kind == Token::Kind::Ident && to_upper(cur->text) == "A") {
            cur = cur->next;
        }
        Op op = Op::Not;
        if (mnem == "SHL") op = Op::Shl;
        else if (mnem == "SHR") op = Op::Shr;
        else if (mnem == "INC") op = Op::Inc;
        else if (mnem == "DEC") op = Op::Dec;
        return emit1(op) && expect_end_of_line(cur, line, err);
    }

    if (mnem == "ADD" || mnem == "SUB" || mnem == "AND" || mnem == "OR" ||
        mnem == "XOR" || mnem == "CMP" || mnem == "MUL") {
        if (cur->kind != Token::Kind::Ident || to_upper(cur->text) != "A") {
            err = line_err(line, mnem + " expects first operand A");
            return false;
        }
        cur = cur->next;
        if (cur->kind != Token::Kind::Comma) {
            err = line_err(line, "expected ',' after A");
            return false;
        }
        cur = cur->next;
        if (cur->kind != Token::Kind::Ident || to_upper(cur->text) != "B") {
            err = line_err(line, mnem + " expects second operand B");
            return false;
        }
        cur = cur->next;
        Op op = Op::Add;
        if (mnem == "SUB") op = Op::Sub;
        else if (mnem == "AND") op = Op::And;
        else if (mnem == "OR")  op = Op::Or;
        else if (mnem == "XOR") op = Op::Xor;
        else if (mnem == "CMP") op = Op::Cmp;
        else if (mnem == "MUL") op = Op::Mul;
        return emit1(op) && expect_end_of_line(cur, line, err);
    }

    if (mnem == "MOV") {
        if (cur->kind != Token::Kind::Ident) {
            err = line_err(line, "MOV expects destination register");
            return false;
        }
        std::string dst = to_upper(cur->text);
        cur = cur->next;
        if (cur->kind != Token::Kind::Comma) {
            err = line_err(line, "expected ',' in MOV");
            return false;
        }
        cur = cur->next;
        if (cur->kind != Token::Kind::Ident) {
            err = line_err(line, "MOV expects source register");
            return false;
        }
        std::string src_reg = to_upper(cur->text);
        cur = cur->next;
        if (dst == "A" && src_reg == "B") {
            return emit1(Op::MovAB) && expect_end_of_line(cur, line, err);
        }
        if (dst == "B" && src_reg == "A") {
            return emit1(Op::MovBA) && expect_end_of_line(cur, line, err);
        }
        err = line_err(line, "invalid MOV operands");
        return false;
    }

    if (mnem == "PUSH" || mnem == "POP") {
        if (cur->kind != Token::Kind::Ident) {
            err = line_err(line, mnem + " expects register A, B, or H");
            return false;
        }
        std::string r = to_upper(cur->text);
        cur = cur->next;
        if (mnem == "PUSH") {
            if (r == "A") return emit1(Op::PushA) && expect_end_of_line(cur, line, err);
            if (r == "B") return emit1(Op::PushB) && expect_end_of_line(cur, line, err);
            if (r == "H") return emit1(Op::PushH) && expect_end_of_line(cur, line, err);
        } else {
            if (r == "A") return emit1(Op::PopA) && expect_end_of_line(cur, line, err);
            if (r == "B") return emit1(Op::PopB) && expect_end_of_line(cur, line, err);
            if (r == "H") return emit1(Op::PopH) && expect_end_of_line(cur, line, err);
        }
        err = line_err(line, "invalid register for " + mnem);
        return false;
    }

    if (mnem == "LOADI") {
        if (cur->kind != Token::Kind::Ident) {
            err = line_err(line, "LOADI expects register A or B");
            return false;
        }
        std::string r = to_upper(cur->text);
        cur = cur->next;
        if (cur->kind != Token::Kind::Comma) {
            err = line_err(line, "expected ',' in LOADI");
            return false;
        }
        cur = cur->next;
        Byte imm = 0;
        if (!resolve_u8(cur, imm, err)) {
            return false;
        }
        cur = cur->next;
        if (r == "A") return emit2(Op::LoadiA, imm) && expect_end_of_line(cur, line, err);
        if (r == "B") return emit2(Op::LoadiB, imm) && expect_end_of_line(cur, line, err);
        err = line_err(line, "LOADI supports only A or B");
        return false;
    }

    if (mnem == "LOADH") {
        if (cur->kind != Token::Kind::Ident || to_upper(cur->text) != "H") {
            err = line_err(line, "LOADH expects register H");
            return false;
        }
        cur = cur->next;
        if (cur->kind != Token::Kind::Comma) {
            err = line_err(line, "expected ',' in LOADH");
            return false;
        }
        cur = cur->next;
        std::uint16_t addr = 0;
        if (!resolve_u16(cur, labels, pass2, addr, err)) {
            return false;
        }
        cur = cur->next;
        return emit3(Op::LoadH, addr) && expect_end_of_line(cur, line, err);
    }

    if (mnem == "LOAD") {
        if (cur->kind != Token::Kind::Ident) {
            err = line_err(line, "LOAD expects destination register A or B");
            return false;
        }
        std::string r = to_upper(cur->text);
        cur = cur->next;
        if (cur->kind != Token::Kind::Comma) {
            err = line_err(line, "expected ',' in LOAD");
            return false;
        }
        cur = cur->next;
        if (cur->kind != Token::Kind::LBracket) {
            err = line_err(line, "expected '[' in LOAD");
            return false;
        }
        cur = cur->next;
        if (cur->kind == Token::Kind::Ident && to_upper(cur->text) == "H") {
            cur = cur->next;
            if (cur->kind != Token::Kind::RBracket) {
                err = line_err(line, "expected ']' after H");
                return false;
            }
            cur = cur->next;
            if (r != "A") {
                err = line_err(line, "LOAD [H] supports only destination A");
                return false;
            }
            return emit1(Op::LoadAH) && expect_end_of_line(cur, line, err);
        }
        std::uint16_t addr = 0;
        if (!resolve_u16(cur, labels, pass2, addr, err)) {
            return false;
        }
        cur = cur->next;
        if (cur->kind != Token::Kind::RBracket) {
            err = line_err(line, "expected ']' after address");
            return false;
        }
        cur = cur->next;
        if (r == "A") return emit3(Op::LoadA, addr) && expect_end_of_line(cur, line, err);
        if (r == "B") return emit3(Op::LoadB, addr) && expect_end_of_line(cur, line, err);
        err = line_err(line, "LOAD supports only A or B");
        return false;
    }

    if (mnem == "STORE") {
        if (cur->kind != Token::Kind::LBracket) {
            err = line_err(line, "STORE expects '['");
            return false;
        }
        cur = cur->next;
        bool indirect_h = false;
        std::uint16_t addr = 0;
        if (cur->kind == Token::Kind::Ident && to_upper(cur->text) == "H") {
            indirect_h = true;
            cur = cur->next;
        } else {
            if (!resolve_u16(cur, labels, pass2, addr, err)) {
                return false;
            }
            cur = cur->next;
        }
        if (cur->kind != Token::Kind::RBracket) {
            err = line_err(line, "expected ']' in STORE");
            return false;
        }
        cur = cur->next;
        if (cur->kind != Token::Kind::Comma) {
            err = line_err(line, "expected ',' in STORE");
            return false;
        }
        cur = cur->next;
        if (cur->kind != Token::Kind::Ident) {
            err = line_err(line, "STORE expects source register A or B");
            return false;
        }
        std::string r = to_upper(cur->text);
        cur = cur->next;
        if (indirect_h) {
            if (r != "A") {
                err = line_err(line, "STORE [H] supports only source register A");
                return false;
            }
            return emit1(Op::StoreHA) && expect_end_of_line(cur, line, err);
        }
        if (r == "A") return emit3(Op::StoreA, addr) && expect_end_of_line(cur, line, err);
        if (r == "B") return emit3(Op::StoreB, addr) && expect_end_of_line(cur, line, err);
        err = line_err(line, "STORE supports only A or B");
        return false;
    }

    if (mnem == "OUTS" || mnem == "JMP" || mnem == "JZ" || mnem == "JNZ" ||
        mnem == "JC" || mnem == "JNC" || mnem == "CALL") {
        std::uint16_t addr = 0;
        if (!resolve_u16(cur, labels, pass2, addr, err)) {
            return false;
        }
        cur = cur->next;
        Op op = Op::Jmp;
        if (mnem == "OUTS") op = Op::Outs;
        else if (mnem == "JZ")   op = Op::Jz;
        else if (mnem == "JNZ")  op = Op::Jnz;
        else if (mnem == "JC")   op = Op::Jc;
        else if (mnem == "JNC")  op = Op::Jnc;
        else if (mnem == "CALL") op = Op::Call;
        return emit3(op, addr) && expect_end_of_line(cur, line, err);
    }

    if (mnem == "DB") {
        while (true) {
            Byte val = 0;
            if (!resolve_u8(cur, val, err)) {
                return false;
            }
            cur = cur->next;
            if (pc >= MEM_SIZE) {
                err = line_err(line, "data exceeds 4096 bytes");
                return false;
            }
            if (pass2) {
                mem_set(mem, pc, val);
            }
            pc = static_cast<std::uint16_t>(pc + 1);
            if (cur->kind == Token::Kind::Comma) {
                cur = cur->next;
            } else {
                break;
            }
        }
        return expect_end_of_line(cur, line, err);
    }

    err = line_err(line, "unknown mnemonic '" + mnem + "'");
    return false;
}

static bool run_pass(Token* head,
                     std::uint16_t base_addr,
                     Memory& mem,
                     LabelNode*& labels,
                     bool pass2,
                     std::uint16_t& end_pc,
                     std::string& err) {
    Token* cur = head;
    std::uint16_t pc = base_addr;

    while (cur && cur->kind != Token::Kind::Eof) {
        if (cur->kind == Token::Kind::Newline) {
            cur = cur->next;
            continue;
        }

        if (cur->kind == Token::Kind::Ident && cur->next && cur->next->kind == Token::Kind::Colon) {
            std::string label_name = cur->text;
            int line = cur->line;
            cur = cur->next->next;
            if (!pass2) {
                if (!add_label(labels, label_name, pc)) {
                    err = line_err(line, "duplicate label '" + label_name + "'");
                    return false;
                }
            }
            if (cur->kind == Token::Kind::Newline) {
                cur = cur->next;
                continue;
            }
            if (cur->kind == Token::Kind::Eof) {
                break;
            }
        }

        if (cur->kind != Token::Kind::Ident) {
            err = line_err(cur->line, "expected instruction or label, got '" + cur->text + "'");
            return false;
        }

        if (!encode_instruction(cur, pc, mem, labels, pass2, err)) {
            return false;
        }
    }

    end_pc = pc;
    return true;
}

AsmResult assemble_source(const std::string& src, Memory& mem, std::uint16_t base_addr) {
    AsmResult res;
    std::string lex_err;
    Token* tokens = lex(src, lex_err);
    if (!tokens) {
        res.error = lex_err;
        return res;
    }

    LabelNode* labels = nullptr;
    std::uint16_t end_pc = base_addr;
    std::string pass_err;

    if (!run_pass(tokens, base_addr, mem, labels, false, end_pc, pass_err)) {
        destroy_labels(labels);
        destroy(tokens);
        res.error = pass_err;
        return res;
    }

    if (!run_pass(tokens, base_addr, mem, labels, true, end_pc, pass_err)) {
        destroy_labels(labels);
        destroy(tokens);
        res.error = pass_err;
        return res;
    }

    destroy_labels(labels);
    destroy(tokens);
    res.ok = true;
    res.bytes_written = static_cast<std::size_t>(end_pc - base_addr);
    return res;
}

void disassemble_range(const Memory& mem, std::uint16_t lo, std::uint16_t hi) {
    std::uint16_t pc = lo;
    while (pc < hi && pc < MEM_SIZE) {
        Byte raw = mem_get(mem, pc);
        Op op = static_cast<Op>(raw);
        std::cout << std::hex << std::setfill('0') << std::setw(4) << pc << "  ";

        auto print_1 = [&](const char* name) {
            std::cout << std::setw(2) << static_cast<int>(raw) << "        " << name << "\n";
            pc = static_cast<std::uint16_t>(pc + 1);
        };
        auto print_2 = [&](const char* name) {
            Byte imm = mem_get(mem, pc + 1);
            std::cout << std::setw(2) << static_cast<int>(raw) << " "
                      << std::setw(2) << static_cast<int>(imm) << "     "
                      << name << "0x" << std::setw(2) << static_cast<int>(imm) << "\n";
            pc = static_cast<std::uint16_t>(pc + 2);
        };
        auto print_3 = [&](const char* prefix, const char* suffix) {
            Byte b1 = mem_get(mem, pc + 1);
            Byte b2 = mem_get(mem, pc + 2);
            std::uint16_t addr = get16(mem, pc + 1);
            std::cout << std::setw(2) << static_cast<int>(raw) << " "
                      << std::setw(2) << static_cast<int>(b1) << " "
                      << std::setw(2) << static_cast<int>(b2) << "  "
                      << prefix << "0x" << std::setw(4) << addr << suffix << "\n";
            pc = static_cast<std::uint16_t>(pc + 3);
        };

        switch (op) {
            case Op::Halt:    print_1("HALT"); break;
            case Op::Nop:     print_1("NOP"); break;
            case Op::Out:     print_1("OUT"); break;
            case Op::Outn:    print_1("OUTN"); break;
            case Op::Outs:    print_3("OUTS ", ""); break;
            case Op::Add:     print_1("ADD A, B"); break;
            case Op::Sub:     print_1("SUB A, B"); break;
            case Op::And:     print_1("AND A, B"); break;
            case Op::Or:      print_1("OR A, B"); break;
            case Op::Xor:     print_1("XOR A, B"); break;
            case Op::Not:     print_1("NOT A"); break;
            case Op::Shl:     print_1("SHL A"); break;
            case Op::Shr:     print_1("SHR A"); break;
            case Op::Inc:     print_1("INC A"); break;
            case Op::Dec:     print_1("DEC A"); break;
            case Op::Cmp:     print_1("CMP A, B"); break;
            case Op::Mul:     print_1("MUL A, B"); break;
            case Op::LoadiA:  print_2("LOADI A, "); break;
            case Op::LoadiB:  print_2("LOADI B, "); break;
            case Op::LoadA:   print_3("LOAD A, [", "]"); break;
            case Op::LoadB:   print_3("LOAD B, [", "]"); break;
            case Op::StoreA:  print_3("STORE [", "], A"); break;
            case Op::StoreB:  print_3("STORE [", "], B"); break;
            case Op::MovAB:   print_1("MOV A, B"); break;
            case Op::MovBA:   print_1("MOV B, A"); break;
            case Op::LoadH:   print_3("LOADH H, ", ""); break;
            case Op::LoadAH:  print_1("LOAD A, [H]"); break;
            case Op::StoreHA: print_1("STORE [H], A"); break;
            case Op::IncH:    print_1("INCH"); break;
            case Op::DecH:    print_1("DECH"); break;
            case Op::HLow:    print_1("HLOW"); break;
            case Op::Jmp:     print_3("JMP ", ""); break;
            case Op::Jz:      print_3("JZ ", ""); break;
            case Op::Jnz:     print_3("JNZ ", ""); break;
            case Op::Jc:      print_3("JC ", ""); break;
            case Op::Jnc:     print_3("JNC ", ""); break;
            case Op::Cls:     print_1("CLS"); break;
            case Op::Plot:    print_1("PLOT"); break;
            case Op::Unplot:  print_1("UNPLOT"); break;
            case Op::Show:    print_1("SHOW"); break;
            case Op::PushA:   print_1("PUSH A"); break;
            case Op::PushB:   print_1("PUSH B"); break;
            case Op::PopA:    print_1("POP A"); break;
            case Op::PopB:    print_1("POP B"); break;
            case Op::Call:    print_3("CALL ", ""); break;
            case Op::Ret:     print_1("RET"); break;
            case Op::PushH:   print_1("PUSH H"); break;
            case Op::PopH:    print_1("POP H"); break;
            case Op::Alloc:   print_1("ALLOC"); break;
            default:
                std::cout << std::setw(2) << static_cast<int>(raw) << "        DB 0x"
                          << std::setw(2) << static_cast<int>(raw) << "\n";
                pc = static_cast<std::uint16_t>(pc + 1);
                break;
        }
    }
    std::cout << std::dec;
}