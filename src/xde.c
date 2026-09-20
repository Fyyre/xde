// XDE v2.00 by Fyyre - eXtended disassembler engine (x86 / x86-64 / VEX / EVEX / XOP)

// Successor to z0mbie's XDE 1.02: instruction length, split/merge, and source/destination object sets.
// Original 1.02 sources present in xde102.

#include "xde.h"
#include "xdetbl.h"

#include <string.h>

typedef struct {
    const uint8_t *beg;
    const uint8_t *p;
    const uint8_t *end;
} xde_cur;

static int cur_left(const xde_cur *c)
{
    size_t a = (size_t)(c->end - c->p);
    size_t b = (size_t)(c->beg + XDE_MAXLEN - c->p);
    return (int)(a < b ? a : b);
}

static int get_byte(xde_cur *c, uint8_t *out)
{
    if (cur_left(c) < 1)
        return 0;
    *out = *c->p++;
    return 1;
}

static int peek_byte(const xde_cur *c, unsigned off, uint8_t *out)
{
    if (cur_left(c) < (int)off + 1)
        return 0;
    *out = c->p[off];
    return 1;
}

static uint64_t gp_set(int sz, unsigned reg, int rex)
{
    static const uint64_t lo8_norex[8] = {
        XSET_AL, XSET_CL, XSET_DL, XSET_BL,
        XSET_AH, XSET_CH, XSET_DH, XSET_BH
    };
    static const uint64_t lo8_rex[8] = {
        XSET_AL, XSET_CL, XSET_DL, XSET_BL,
        XSET_SP, XSET_BP, XSET_SI, XSET_DI
    };
    static const uint64_t w16[8] = {
        XSET_AX, XSET_CX, XSET_DX, XSET_BX,
        XSET_SP, XSET_BP, XSET_SI, XSET_DI
    };
    static const uint64_t w32[8] = {
        XSET_EAX, XSET_ECX, XSET_EDX, XSET_EBX,
        XSET_ESP, XSET_EBP, XSET_ESI, XSET_EDI
    };
    static const uint64_t w64[8] = {
        XSET_RAX, XSET_RCX, XSET_RDX, XSET_RBX,
        XSET_RSP, XSET_RBP, XSET_RSI, XSET_RDI
    };

    if (reg >= 16)
        return XSET_OTHER;
    if (reg >= 8)
        return XSET_R8 << (reg - 8);

    if (sz <= 1)
        return rex ? lo8_rex[reg] : lo8_norex[reg];
    if (sz == 2)
        return w16[reg];
    if (sz == 4)
        return w32[reg];
    return w64[reg];
}

static uint64_t stack_set(unsigned mode)
{
    if (mode == 16)
        return XSET_SP;
    if (mode == 32)
        return XSET_ESP;
    return XSET_RSP;
}

static unsigned imm_bytes(uint32_t attr, const struct xde_instr *diza)
{
    unsigned kind = (attr & XA_IMM_MASK);
    unsigned data = diza->defdata;

    switch (kind) {
    case XA_IMM_IB:     return 1;
    case XA_IMM_IW:     return 2;
    case XA_IMM_IZ:
        if (diza->mode == 64 && (attr & XA_F64))
            return 4;
        return data == 2 ? 2u : 4u;
    case XA_IMM_IV:
        return data;
    case XA_IMM_AP:
        return (data == 2 ? 2u : 4u) + 2u;
    case XA_IMM_ENTER:  return 3;
    case XA_IMM_ID:     return 4;
    default:            return 0;
    }
}

static void apply_attr_flags(struct xde_instr *diza, uint32_t attr)
{
    uint64_t f = diza->flag;
    if (attr & XA_MODRM)  f |= C_MODRM;
    if (attr & XA_REL)    f |= C_REL;
    if (attr & XA_STOP)   f |= C_STOP;
    if (attr & XA_UNDEF)  f |= C_UNDEF;
    if (attr & XA_BAD)    f |= C_BAD;
    if (attr & XA_OPSZ8)  f |= C_OPSZ8;
    if (attr & XA_PUSH)   f |= C_PUSH;
    if (attr & XA_POP)    f |= C_POP;
    if (attr & XA_I64)    f |= C_I64;
    if (attr & XA_O64)    f |= C_O64;
    if (attr & XA_F64)    f |= C_F64;
    if (attr & XA_D64)    f |= C_D64;
    if (attr & XA_3DNOW)  f |= C_3DNOW;
    if (attr & XA_CALL)   f |= C_CMD_CALL;
    if (attr & XA_JMP)    f |= C_CMD_JMP;
    if (attr & XA_JCC)    f |= C_CMD_JCC;
    if (attr & XA_RET)    f |= C_CMD_RET;
    diza->flag = f;
}

static void apply_usage_special(struct xde_instr *diza, uint32_t attr,
                                unsigned opcode, unsigned opcode2)
{
    unsigned mode = diza->mode;
    unsigned data = diza->defdata;
    unsigned addr = diza->defaddr;
    uint64_t xset;
    uint8_t c = (uint8_t)opcode;

    // REP/REPE/REPNE: CX/ECX/RCX is src and dst. Flags are read for CMPS/SCAS
    // termination; other reps do not write flags.
    if (diza->p_rep) {
        xset = (mode == 64) ? XSET_RCX : (addr == 2 ? XSET_CX : XSET_ECX);
        diza->src_set |= xset;
        diza->dst_set |= xset;
        diza->src_set |= XSET_FL;
    }

    if (diza->map == XDE_MAP_LEGACY) {
        if ((c == 0xA4) || (c == 0xA5) || (c == 0xA6) || (c == 0xA7)) {
            xset = addr == 2 ? (XSET_SI | XSET_DI)
                 : addr == 4 ? (XSET_ESI | XSET_EDI)
                             : (XSET_RSI | XSET_RDI);
            diza->src_set |= xset;
            diza->dst_set |= xset;
        }
        if ((c == 0xAC) || (c == 0xAD)) {
            xset = addr == 2 ? XSET_SI : addr == 4 ? XSET_ESI : XSET_RSI;
            diza->src_set |= xset;
            diza->dst_set |= xset;
        }
        if ((c == 0xAA) || (c == 0xAB) || (c == 0xAE) || (c == 0xAF)) {
            xset = addr == 2 ? XSET_DI : addr == 4 ? XSET_EDI : XSET_RDI;
            diza->src_set |= xset;
            diza->dst_set |= xset;
        }
        if ((c == 0x6C) || (c == 0x6D)) {
            xset = XSET_DEV | (addr == 2 ? XSET_DI : addr == 4 ? XSET_EDI : XSET_RDI);
            diza->src_set |= xset | XSET_DX;
            diza->dst_set |= xset;
        }
        if ((c == 0x6E) || (c == 0x6F)) {
            xset = XSET_DEV | (addr == 2 ? XSET_SI : addr == 4 ? XSET_ESI : XSET_RSI);
            diza->src_set |= xset | XSET_DX;
            diza->dst_set |= xset;
        }
        if (c == 0x9E) diza->src_set |= XSET_AH;
        if (c == 0x9F) diza->dst_set |= XSET_AH;
        if (c == 0x98) {
            if (data == 2) { diza->src_set |= XSET_AL;  diza->dst_set |= XSET_AX; }
            else if (data == 4) { diza->src_set |= XSET_AX;  diza->dst_set |= XSET_EAX; }
            else { diza->src_set |= XSET_EAX; diza->dst_set |= XSET_RAX; }
        }
        if (c == 0x99) {
            if (data == 2) { diza->src_set |= XSET_AX;  diza->dst_set |= XSET_DX; }
            else if (data == 4) { diza->src_set |= XSET_EAX; diza->dst_set |= XSET_EDX; }
            else { diza->src_set |= XSET_RAX; diza->dst_set |= XSET_RDX; }
        }
        if ((c == 0x37) || (c == 0x3F)) {
            diza->src_set |= XSET_AH;
            diza->dst_set |= XSET_AH;
        }
        if ((c == 0xD4) || (c == 0xD5)) {
            diza->src_set |= (c == 0xD4) ? XSET_AL : XSET_AX;
            diza->dst_set |= XSET_AX;
        }
        if (c == 0x60)
            diza->src_set |= data == 2 ? XSET_ALL16 : XSET_ALL32;
        if (c == 0x61)
            diza->dst_set |= data == 2 ? XSET_ALL16 : XSET_ALL32;
        if ((c == 0xE4) || (c == 0xE5) || (c == 0xE6) || (c == 0xE7) ||
            (c == 0xEC) || (c == 0xED) || (c == 0xEE) || (c == 0xEF)) {
            diza->src_set |= XSET_DEV;
            diza->dst_set |= XSET_DEV;
            if ((c == 0xEC) || (c == 0xED) || (c == 0xEE) || (c == 0xEF)) {
                if ((c == 0xEC) || (c == 0xED))
                    diza->src_set |= XSET_DX;
                else
                    diza->dst_set |= XSET_DX;
            }
        }
        if ((c == 0x06) || (c == 0x0E) || (c == 0x16) || (c == 0x1E))
            diza->src_set |= XSET_OTHER;
        if ((c == 0x07) || (c == 0x17) || (c == 0x1F) || (c == 0xC4) || (c == 0xC5))
            diza->dst_set |= XSET_OTHER;
        if (c == 0xD7)
            diza->src_set |= addr == 2 ? XSET_BX : addr == 4 ? XSET_EBX : XSET_RBX;
        if ((c == 0xC8) || (c == 0xC9)) {
            xset = stack_set(mode) | (mode == 16 ? XSET_BP : mode == 32 ? XSET_EBP : XSET_RBP);
            diza->src_set |= xset;
            diza->dst_set |= xset;
        }
        if (c == 0x8C) diza->src_set |= XSET_OTHER;
        if (c == 0x8E) diza->dst_set |= XSET_OTHER;
    } else if (diza->map == XDE_MAP_0F) {
        uint8_t c2 = (uint8_t)opcode2;
        if ((c2 == 0xB2) || (c2 == 0xB4) || (c2 == 0xB5) || (c2 == 0xA1) || (c2 == 0xA9))
            diza->dst_set |= XSET_OTHER;
        if ((c2 == 0xA0) || (c2 == 0xA8))
            diza->src_set |= XSET_OTHER;
        if (c2 == 0xA2) {
            diza->src_set |= XSET_EAX;
            diza->dst_set |= XSET_EAX | XSET_EBX | XSET_ECX | XSET_EDX;
        }
        if ((c2 == 0xA5) || (c2 == 0xAD))
            diza->src_set |= XSET_CL;
    }

    (void)attr;
}

static void apply_modrm_usage(struct xde_instr *diza, uint32_t attr,
                              unsigned mod, unsigned reg, unsigned rm)
{
    int rex = (diza->rex != 0) || (diza->enc != XDE_ENC_LEGACY);
    int opsz8 = (attr & XA_OPSZ8) != 0;
    int sz = opsz8 ? 1 : (int)diza->defdata;
    int dsz = sz;
    uint8_t c = diza->opcode;
    uint8_t c2 = diza->opcode2;

    // 32-bit GP writes zero-extend in 64-bit mode.
    if (diza->mode == 64 && dsz == 4)
        dsz = 8;

    // Vector encodings use OTHER for the reg field.
    if (diza->enc == XDE_ENC_VEX2 || diza->enc == XDE_ENC_VEX3 ||
        diza->enc == XDE_ENC_EVEX || diza->enc == XDE_ENC_XOP) {
        diza->src_set |= XSET_OTHER;
        diza->dst_set |= XSET_OTHER;
        if (diza->vex_vvvv != 0 && diza->vex_vvvv != 0xF) {
            if (attr & XA_VVVV_GPR)
                diza->src_set |= gp_set(dsz, diza->vex_vvvv, 1);
            else
                diza->src_set |= XSET_OTHER;
        }
    }

    if (c == 0x8B || c == 0x8A || c == 0x8D ||
        (c == 0x0F && (c2 == 0xB6 || c2 == 0xB7 || c2 == 0xBE || c2 == 0xBF ||
                       (c2 >= 0x40 && c2 <= 0x4F) || c2 == 0xAF || c2 == 0xBC || c2 == 0xBD ||
                       c2 == 0xB8))) {
        diza->dst_set |= (diza->enc == XDE_ENC_LEGACY || (attr & XA_VVVV_GPR))
                       ? gp_set(dsz, ((unsigned)diza->rex_r << 3) | reg, rex)
                       : XSET_OTHER;
    }
    if (c == 0x89 || c == 0x88) {
        diza->src_set |= gp_set(sz, ((unsigned)diza->rex_r << 3) | reg, rex);
    }
    if ((c <= 0x3D) && ((c & 7) <= 3) && diza->map == XDE_MAP_LEGACY) {
        unsigned form = c & 7;
        uint64_t rset = gp_set((form & 1) ? dsz : 1, ((unsigned)diza->rex_r << 3) | reg, rex);
        if (form == 0 || form == 1) {
            diza->src_set |= rset;
            diza->dst_set |= XSET_FL;
        } else if (form == 2 || form == 3) {
            diza->src_set |= rset;
            diza->dst_set |= rset | XSET_FL;
        }
    }

    if (mod == 3) {
        unsigned rmreg = ((unsigned)diza->rex_b << 3) | rm;
        uint64_t rset = gp_set(sz, rmreg, rex);
        if (diza->mode == 64 && sz == 4)
            rset = gp_set(8, rmreg, rex);
        if (diza->enc != XDE_ENC_LEGACY && !(attr & XA_VVVV_GPR))
            rset = XSET_OTHER;
        if (c != 0x8D)
            diza->src_set |= rset;
        // dest of r/m for ALU, MOV r/m, shifts, etc.
        if (diza->map == XDE_MAP_LEGACY &&
            ((c <= 0x33 && (c & 7) <= 1) || c == 0x86 || c == 0x87 ||
             c == 0x88 || c == 0x89 || (c >= 0xC0 && c <= 0xC1) ||
             (c >= 0xD0 && c <= 0xD3) || c == 0xF6 || c == 0xF7 ||
             c == 0xFE || c == 0xFF || (c >= 0x80 && c <= 0x83)))
            diza->dst_set |= rset;
        if (c == 0x0F && (c2 == 0xB6 || c2 == 0xB7 || c2 == 0xBE || c2 == 0xBF)) {
            int srcsz = (c2 == 0xB6 || c2 == 0xBE) ? 1 : 2;
            diza->src_set |= gp_set(srcsz, rmreg, rex);
        }
    } else {
        if (c != 0x8D) {
            diza->src_set |= XSET_OTHER; // segment override
            diza->src_set |= XSET_MEM;
        }
        if (diza->map == XDE_MAP_LEGACY &&
            ((c <= 0x33 && (c & 7) <= 1) || c == 0x86 || c == 0x87 ||
             c == 0x88 || c == 0x89 || (c >= 0xC0 && c <= 0xC1) ||
             (c >= 0xD0 && c <= 0xD3) || c == 0xF6 || c == 0xF7 ||
             c == 0xFE || c == 0xFF || (c >= 0x80 && c <= 0x83) ||
             c == 0xC6 || c == 0xC7))
            diza->dst_set |= XSET_MEM;
        else if (diza->enc != XDE_ENC_LEGACY)
            diza->src_set |= XSET_MEM;
    }
}

static void apply_implicit_gp(struct xde_instr *diza, uint32_t attr)
{
    uint8_t c = diza->opcode;
    int rex = (diza->rex != 0);
    int sz = (attr & XA_OPSZ8) ? 1 : (int)diza->defdata;
    int dsz = (diza->mode == 64 && sz == 4) ? 8 : sz;
    uint64_t xset;

    if (diza->map != XDE_MAP_LEGACY && diza->map != XDE_MAP_0F)
        return;

    if (diza->map == XDE_MAP_LEGACY) {
        if ((c & 0xF8) == 0x40 || (c & 0xF8) == 0x48) { // INC/DEC r
            xset = gp_set(dsz, (c & 7) | ((unsigned)diza->rex_b << 3), rex);
            diza->src_set |= xset;
            diza->dst_set |= xset | XSET_FL;
        }
        if ((c & 0xF8) == 0x50)
            diza->src_set |= gp_set(dsz, (c & 7) | ((unsigned)diza->rex_b << 3), rex);
        if ((c & 0xF8) == 0x58)
            diza->dst_set |= gp_set(dsz, (c & 7) | ((unsigned)diza->rex_b << 3), rex);
        if ((c & 0xF8) == 0x90 && c != 0x90) {
            xset = gp_set(dsz, (c & 7) | ((unsigned)diza->rex_b << 3), rex);
            diza->src_set |= xset | gp_set(dsz, ((unsigned)diza->rex_r << 3), rex);
            diza->dst_set |= xset | gp_set(dsz, ((unsigned)diza->rex_r << 3), rex);
        }
        if ((c & 0xF8) == 0xB0)
            diza->dst_set |= gp_set(1, (c & 7) | ((unsigned)diza->rex_b << 3), rex);
        if ((c & 0xF8) == 0xB8)
            diza->dst_set |= gp_set(dsz, (c & 7) | ((unsigned)diza->rex_b << 3), rex);
        if ((c & 7) == 4 && c < 0x3E) { // ALU AL, Ib
            diza->src_set |= XSET_AL;
            diza->dst_set |= XSET_AL | XSET_FL;
            if (c >= 0x38) diza->dst_set &= ~XSET_AL;
        }
        if ((c & 7) == 5 && c < 0x3E) {
            xset = gp_set(dsz, 0, rex);
            diza->src_set |= xset;
            diza->dst_set |= xset | XSET_FL;
            if (c >= 0x38) diza->dst_set &= ~xset;
        }
    }
    if (diza->map == XDE_MAP_0F && (diza->opcode2 & 0xF8) == 0xC8) {
        unsigned r = (diza->opcode2 & 7) | ((unsigned)diza->rex_b << 3);
        xset = gp_set(dsz, r, 1);
        diza->src_set |= xset;
        diza->dst_set |= xset;
    }

    xset = stack_set(diza->mode);
    if (attr & XA_PUSH) {
        diza->src_set |= xset;
        diza->dst_set |= xset | XSET_MEM;
    }
    if (attr & XA_POP) {
        diza->src_set |= xset | XSET_MEM;
        diza->dst_set |= xset;
    }
}

static int parse_modrm(xde_cur *cur, struct xde_instr *diza, uint32_t attr)
{
    uint8_t m, sib;
    unsigned mod, rm, addr;
    unsigned disp = 0;
    unsigned i;

    if (!get_byte(cur, &m))
        return 0;
    diza->modrm = m;
    diza->flag |= C_MODRM;

    mod = m >> 6;
    rm  = m & 7;
    addr = diza->defaddr;

    if (mod == 3)
        return 1;

    if (addr == 2) {
        // 16-bit addressing: no SIB
        if (mod == 1)
            disp = 1;
        else if (mod == 2)
            disp = 2;
        else if (rm == 6)
            disp = 2;
    } else {
        if (rm == 4) {
            if (!get_byte(cur, &sib))
                return 0;
            diza->sib = sib;
            diza->flag |= C_SIB;
            {
                unsigned base = sib & 7;
                unsigned index = (sib >> 3) & 7;
                if (mod == 0 && base == 5)
                    disp = 4;
                if (diza->enc != XDE_ENC_LEGACY && !(attr & XA_VVVV_GPR))
                    diza->src_set |= XSET_OTHER;
                else {
                    if (!(mod == 0 && base == 5) || diza->rex_b)
                        diza->src_set |= gp_set((int)addr,
                            ((unsigned)diza->rex_b << 3) | base, 1);
                    if (index != 4 || diza->rex_x)
                        diza->src_set |= gp_set((int)addr,
                            ((unsigned)diza->rex_x << 3) | index, 1);
                }
            }
        } else if (mod == 0 && rm == 5) {
            disp = 4;
            if (addr == 8) {
                diza->flag |= C_RIPREL;
                diza->src_set |= XSET_RIP;
            }
        } else {
            diza->src_set |= gp_set((int)addr,
                ((unsigned)diza->rex_b << 3) | rm, 1);
        }

        if (mod == 1)
            disp = 1;
        else if (mod == 2)
            disp = 4;
    }

    if (disp) {
        if (cur_left(cur) < (int)disp)
            return 0;
        for (i = 0; i < disp; i++)
            diza->addr_b[i] = *cur->p++;
        diza->addrsize = (uint8_t)disp;
        if (disp == 1) diza->flag |= C_ADDR1;
        else if (disp == 2) diza->flag |= C_ADDR2;
        else if (disp == 4) diza->flag |= C_ADDR4;
        else diza->flag |= C_ADDR8;
    }
    return 1;
}

static int parse_legacy_opcode(xde_cur *cur, struct xde_instr *diza,
                               unsigned *map, uint8_t *mop)
{
    uint8_t b;
    if (!get_byte(cur, &b))
        return 0;
    diza->opcode = b;
    if (b != 0x0F) {
        *map = XDE_MAP_LEGACY;
        *mop = b;
        return 1;
    }
    if (!get_byte(cur, &b))
        return 0;
    diza->opcode2 = b;
    if (b == 0x38) {
        if (!get_byte(cur, &b))
            return 0;
        diza->opcode3 = b;
        *map = XDE_MAP_0F38;
        *mop = b;
        return 1;
    }
    if (b == 0x3A) {
        if (!get_byte(cur, &b))
            return 0;
        diza->opcode3 = b;
        *map = XDE_MAP_0F3A;
        *mop = b;
        return 1;
    }
    *map = XDE_MAP_0F;
    *mop = b;
    return 1;
}

int __cdecl xde_disasm_buf(const uint8_t *opcode, unsigned max_len,
                           struct xde_instr *diza, unsigned mode)
{
    xde_cur cur;
    uint8_t b, b1, b2, b3;
    unsigned map;
    uint8_t mop;
    uint32_t attr, gattr;
    unsigned i, dbytes;
    int twice;

    if (!opcode || !diza)
        return 0;
    if (mode != 16 && mode != 32 && mode != 64)
        return 0;
    if (max_len == 0)
        max_len = XDE_MAXLEN;
    if (max_len > XDE_MAXLEN)
        max_len = XDE_MAXLEN;

    memset(diza, 0, sizeof(*diza));
    diza->mode = (uint8_t)mode;
    diza->defaddr = (uint8_t)(mode == 16 ? 2 : mode == 32 ? 4 : 8);
    diza->defdata = (uint8_t)(mode == 16 ? 2 : 4);

    cur.beg = opcode;
    cur.p = opcode;
    cur.end = opcode + max_len;

    if (max_len >= 2) {
        uint16_t w = (uint16_t)(opcode[0] | (opcode[1] << 8));
        if (w == 0x0000 || w == 0xFFFF)
            diza->flag |= C_BAD;
    }

    // legacy prefixes (groups 1-4)
    for (;;) {
        if (!peek_byte(&cur, 0, &b))
            return 0;
        twice = 0;
        if (b == 0x66) {
            twice = diza->p_66 != 0;
            diza->p_66 = 0x66;
            diza->defdata = (uint8_t)(diza->defdata == 2 ? 4 : 2);
            cur.p++;
            if (twice) diza->flag |= C_BAD;
            continue;
        }
        if (b == 0x67) {
            twice = diza->p_67 != 0;
            diza->p_67 = 0x67;
            if (mode == 64)
                diza->defaddr = (uint8_t)(diza->defaddr == 8 ? 4 : 8);
            else
                diza->defaddr = (uint8_t)(diza->defaddr == 2 ? 4 : 2);
            cur.p++;
            if (twice) diza->flag |= C_BAD;
            continue;
        }
        if (b == 0x26 || b == 0x2E || b == 0x36 || b == 0x3E ||
            b == 0x64 || b == 0x65) {
            twice = diza->p_seg != 0;
            diza->p_seg = b;
            cur.p++;
            if (twice) diza->flag |= C_BAD;
            continue;
        }
        if (b == 0xF2 || b == 0xF3) {
            twice = diza->p_rep != 0;
            diza->p_rep = b;
            cur.p++;
            if (twice) diza->flag |= C_BAD;
            continue;
        }
        if (b == 0xF0) {
            twice = diza->p_lock != 0;
            diza->p_lock = b;
            cur.p++;
            if (twice) diza->flag |= C_BAD;
            continue;
        }
        break;
    }

    // REX / REX2 / VEX / EVEX / XOP
    if (!peek_byte(&cur, 0, &b))
        return 0;

    if (mode == 64 && b >= 0x40 && b <= 0x4F) {
        diza->rex = b;
        diza->rex_w = (uint8_t)((b >> 3) & 1);
        diza->rex_r = (uint8_t)((b >> 2) & 1);
        diza->rex_x = (uint8_t)((b >> 1) & 1);
        diza->rex_b = (uint8_t)(b & 1);
        diza->flag |= C_REX;
        if (diza->rex_w)
            diza->defdata = 8;
        cur.p++;
        if (!peek_byte(&cur, 0, &b))
            return 0;
    }

    if (mode == 64 && b == 0xD5) {
        // REX2: D5 [M R4 X4 B4 W R X B]
        if (!peek_byte(&cur, 1, &b1))
            return 0;
        diza->enc = XDE_ENC_REX2;
        diza->nvex = 2;
        diza->vex[0] = 0xD5;
        diza->vex[1] = b1;
        diza->flag |= C_REX2 | C_REX;
        diza->rex_w = (uint8_t)((b1 >> 3) & 1);
        diza->rex_r = (uint8_t)((b1 >> 2) & 1);
        diza->rex_x = (uint8_t)((b1 >> 1) & 1);
        diza->rex_b = (uint8_t)(b1 & 1);
        if (diza->rex_w)
            diza->defdata = 8;
        cur.p += 2;
        map = (b1 & 0x80) ? XDE_MAP_0F : XDE_MAP_LEGACY;
        if (!get_byte(&cur, &mop))
            return 0;
        diza->opcode = mop;
        if (map == XDE_MAP_0F)
            diza->opcode2 = mop;
        diza->map = (uint8_t)map;
        goto got_opcode;
    }

    if (b == 0x62) {
        // Need four bytes to even consider EVEX. otherwise this is BOUND.
        if (peek_byte(&cur, 1, &b1) && peek_byte(&cur, 2, &b2) && peek_byte(&cur, 3, &b3) &&
            (b2 & 0x04) && (mode == 64 || (b1 & 0xC0) == 0xC0)) {
            // EVEX: P1.bit2 must be 1. In 32-bit, P0.mod must be 11b (else BOUND).
            diza->enc = XDE_ENC_EVEX;
            diza->nvex = 4;
            diza->vex[0] = 0x62;
            diza->vex[1] = b1;
            diza->vex[2] = b2;
            diza->vex[3] = b3;
            diza->flag |= C_EVEX | C_VEX;
            diza->rex_r  = (uint8_t)((~b1 >> 7) & 1);
            diza->rex_x  = (uint8_t)((~b1 >> 6) & 1);
            diza->rex_b  = (uint8_t)((~b1 >> 5) & 1);
            diza->evex_r2 = (uint8_t)((~b1 >> 4) & 1);
            map = b1 & 7;
            diza->rex_w = (uint8_t)((b2 >> 7) & 1);
            diza->vex_vvvv = (uint8_t)((~b2 >> 3) & 0xF);
            diza->vex_pp = (uint8_t)(b2 & 3);
            diza->evex_z = (uint8_t)((b3 >> 7) & 1);
            diza->vex_l  = (uint8_t)((b3 >> 5) & 3);
            diza->evex_b = (uint8_t)((b3 >> 4) & 1);
            diza->evex_aaa = (uint8_t)(b3 & 7);
            if (!((b3 >> 3) & 1)) // V' == 0 extends vvvv
                diza->vex_vvvv |= 0x10;
            diza->defdata = (uint8_t)(mode == 16 ? 2 : 4);
            if (diza->rex_w)
                diza->defdata = 8;
            // pp implied 66/F3/F2 is not a legacy prefix
            diza->p_66 = 0;
            diza->p_rep = 0;
            cur.p += 4;
            if (!get_byte(&cur, &mop))
                return 0;
            diza->opcode = mop;
            diza->map = (uint8_t)map;
            if (map == XDE_MAP_0F)
                diza->opcode2 = mop;
            else if (map == XDE_MAP_0F38) {
                diza->opcode2 = 0x38;
                diza->opcode3 = mop;
            } else if (map == XDE_MAP_0F3A) {
                diza->opcode2 = 0x3A;
                diza->opcode3 = mop;
            }
            goto got_opcode;
        }
        // fall through: BOUND in 16/32-bit, or a truncated 0x62
    }

    if (b == 0xC4 || b == 0xC5) {
        if (!peek_byte(&cur, 1, &b1))
            return 0;
        if (mode == 64 || (b1 & 0xC0) == 0xC0) {
            if (b == 0xC5) {
                diza->enc = XDE_ENC_VEX2;
                diza->nvex = 2;
                diza->vex[0] = 0xC5;
                diza->vex[1] = b1;
                diza->flag |= C_VEX;
                diza->rex_r = (uint8_t)((~b1 >> 7) & 1);
                diza->vex_vvvv = (uint8_t)((~b1 >> 3) & 0xF);
                diza->vex_l = (uint8_t)((b1 >> 2) & 1);
                diza->vex_pp = (uint8_t)(b1 & 3);
                map = XDE_MAP_0F;
                diza->defdata = (uint8_t)(mode == 16 ? 2 : 4);
                diza->p_66 = 0;
                diza->p_rep = 0;
                cur.p += 2;
            } else {
                if (!peek_byte(&cur, 2, &b2))
                    return 0;
                diza->enc = XDE_ENC_VEX3;
                diza->nvex = 3;
                diza->vex[0] = 0xC4;
                diza->vex[1] = b1;
                diza->vex[2] = b2;
                diza->flag |= C_VEX;
                diza->rex_r = (uint8_t)((~b1 >> 7) & 1);
                diza->rex_x = (uint8_t)((~b1 >> 6) & 1);
                diza->rex_b = (uint8_t)((~b1 >> 5) & 1);
                map = b1 & 0x1F;
                diza->rex_w = (uint8_t)((b2 >> 7) & 1);
                diza->vex_vvvv = (uint8_t)((~b2 >> 3) & 0xF);
                diza->vex_l = (uint8_t)((b2 >> 2) & 1);
                diza->vex_pp = (uint8_t)(b2 & 3);
                diza->defdata = (uint8_t)(mode == 16 ? 2 : 4);
                if (diza->rex_w)
                    diza->defdata = 8;
                diza->p_66 = 0;
                diza->p_rep = 0;
                cur.p += 3;
            }
            if (!get_byte(&cur, &mop))
                return 0;
            diza->opcode = mop;
            diza->map = (uint8_t)map;
            if (map == XDE_MAP_0F)
                diza->opcode2 = mop;
            else if (map == XDE_MAP_0F38) {
                diza->opcode2 = 0x38;
                diza->opcode3 = mop;
            } else if (map == XDE_MAP_0F3A) {
                diza->opcode2 = 0x3A;
                diza->opcode3 = mop;
            }
            goto got_opcode;
        }
    }

    if (b == 0x8F) {
        if (!peek_byte(&cur, 1, &b1))
            return 0;
        if ((b1 & 0x1F) >= 8) {
            if (!peek_byte(&cur, 2, &b2))
                return 0;
            diza->enc = XDE_ENC_XOP;
            diza->nvex = 3;
            diza->vex[0] = 0x8F;
            diza->vex[1] = b1;
            diza->vex[2] = b2;
            diza->flag |= C_XOP;
            diza->rex_r = (uint8_t)((~b1 >> 7) & 1);
            diza->rex_x = (uint8_t)((~b1 >> 6) & 1);
            diza->rex_b = (uint8_t)((~b1 >> 5) & 1);
            map = b1 & 0x1F;
            diza->rex_w = (uint8_t)((b2 >> 7) & 1);
            diza->vex_vvvv = (uint8_t)((~b2 >> 3) & 0xF);
            diza->vex_l = (uint8_t)((b2 >> 2) & 1);
            diza->vex_pp = (uint8_t)(b2 & 3);
            diza->defdata = (uint8_t)(mode == 16 ? 2 : 4);
            if (diza->rex_w)
                diza->defdata = 8;
            diza->p_66 = 0;
            diza->p_rep = 0;
            cur.p += 3;
            if (!get_byte(&cur, &mop))
                return 0;
            diza->opcode = mop;
            diza->map = (uint8_t)map;
            goto got_opcode;
        }
    }

    if (!parse_legacy_opcode(&cur, diza, &map, &mop))
        return 0;
    diza->map = (uint8_t)map;
    diza->enc = XDE_ENC_LEGACY;

got_opcode:
    if (map >= XDE_MAP_COUNT)
        return 0;
    attr = xde_attr[map][mop];

    if (attr & XA_INVALID)
        return 0;
    if ((attr & XA_I64) && mode == 64)
        return 0;
    if ((attr & XA_O64) && mode != 64)
        return 0;

    apply_attr_flags(diza, attr);

    // Group extra (immediate / CALL / JMP) after ModR/M.reg
    if (attr & XA_GROUP) {
        if (!(attr & XA_MODRM))
            attr |= XA_MODRM;
    }

    if (attr & XA_MODRM) {
        uint8_t mpeek;
        unsigned reg;
        if (!peek_byte(&cur, 0, &mpeek))
            return 0;
        reg = (mpeek >> 3) & 7;
        if (attr & XA_GROUP) {
            unsigned gid = XA_GRP_ID(attr);
            if (gid < XG_COUNT) {
                gattr = xde_group[gid][reg];
                attr |= gattr;
                apply_attr_flags(diza, gattr);
            }
        }
        if ((diza->opcode == 0xC0 || diza->opcode == 0xC1 ||
             (diza->opcode >= 0xD0 && diza->opcode <= 0xD3)) &&
            diza->map == XDE_MAP_LEGACY) {
            if (reg == 2 || reg == 3)
                diza->src_set |= XSET_FL;
            if (diza->opcode == 0xD2 || diza->opcode == 0xD3)
                diza->src_set |= XSET_CL;
        }
        if ((diza->opcode == 0xC6 || diza->opcode == 0xC7 || diza->opcode == 0x8F) &&
            diza->map == XDE_MAP_LEGACY && reg != 0)
            diza->flag |= C_BAD;
        if (diza->opcode == 0xF6 && diza->map == XDE_MAP_LEGACY) {
            if (reg == 4 || reg == 5) {
                diza->src_set |= XSET_AL;
                diza->dst_set |= XSET_AX;
            }
            if (reg == 6 || reg == 7) {
                diza->src_set |= XSET_AX;
                diza->dst_set |= XSET_AX;
            }
        }
        if (diza->opcode == 0xF7 && diza->map == XDE_MAP_LEGACY) {
            int sz = (int)diza->defdata;
            uint64_t acc = gp_set(sz, 0, diza->rex != 0);
            uint64_t dx  = gp_set(sz, 2, diza->rex != 0);
            if (reg == 4 || reg == 5) {
                diza->src_set |= acc;
                diza->dst_set |= acc | dx | XSET_FL;
            }
            if (reg == 6 || reg == 7) {
                diza->src_set |= acc | dx;
                diza->dst_set |= acc | dx | XSET_FL;
            }
        }

        if (!parse_modrm(&cur, diza, attr))
            return 0;
        apply_modrm_usage(diza, attr, diza->modrm >> 6,
                          (diza->modrm >> 3) & 7, diza->modrm & 7);
    } else if (attr & XA_MOFFS) {
        unsigned asz = diza->defaddr;
        if (cur_left(&cur) < (int)asz)
            return 0;
        for (i = 0; i < asz; i++)
            diza->addr_b[i] = *cur.p++;
        diza->addrsize = (uint8_t)asz;
        diza->flag |= C_ADDR67;
        if (asz == 8) diza->flag |= C_ADDR8;
        else if (asz == 4) diza->flag |= C_ADDR4;
        else if (asz == 2) diza->flag |= C_ADDR2;
        diza->src_set |= XSET_MEM;
        diza->dst_set |= (diza->opcode == 0xA0 || diza->opcode == 0xA1) ? 0 : XSET_MEM;
    }

    apply_usage_special(diza, attr, diza->opcode, diza->opcode2);
    apply_implicit_gp(diza, attr);

    if (attr & XA_UNDEF) {
        diza->src_set = XSET_UNDEF;
        diza->dst_set = XSET_UNDEF;
    }

    dbytes = imm_bytes(attr, diza);
    if (dbytes) {
        if (cur_left(&cur) < (int)dbytes)
            return 0;
        for (i = 0; i < dbytes; i++)
            diza->data_b[i] = *cur.p++;
        diza->datasize = (uint8_t)dbytes;
        if (dbytes == 1) diza->flag |= C_DATA1;
        else if (dbytes == 2) diza->flag |= C_DATA2;
        else if (dbytes == 3) diza->flag |= C_DATA1 | C_DATA2;
        else if (dbytes == 4) diza->flag |= C_DATA4;
        else if (dbytes == 8) diza->flag |= C_DATA8;
        else if (dbytes == 6) diza->flag |= C_DATA4 | C_DATA2;
    }

    // 3DNow: extra opcode byte after operands is already counted as Ib.
    {
        unsigned len = (unsigned)(cur.p - opcode);
        if (len == 0 || len > XDE_MAXLEN)
            return 0;
        diza->len = (uint8_t)len;
        diza->flag |= (attr & XA_REL) ? C_REL : 0;
        return (int)len;
    }
}

int __cdecl xde_disasm_ex(const uint8_t *opcode, struct xde_instr *diza, unsigned mode)
{
    return xde_disasm_buf(opcode, XDE_MAXLEN, diza, mode);
}

int __cdecl xde_disasm(const uint8_t *opcode, struct xde_instr *diza)
{
    return xde_disasm_buf(opcode, XDE_MAXLEN, diza, XDE_MODE_64);
}

int __cdecl xde_asm(uint8_t *opcode, const struct xde_instr *diza)
{
    uint8_t *p;
    unsigned i;

    if (!opcode || !diza)
        return 0;
    p = opcode;

    if (diza->p_seg)  *p++ = diza->p_seg;
    if (diza->p_lock) *p++ = diza->p_lock;
    if (diza->p_rep)  *p++ = diza->p_rep;
    if (diza->p_67)   *p++ = diza->p_67;

    if (diza->nvex) {
        for (i = 0; i < diza->nvex; i++)
            *p++ = diza->vex[i];
        *p++ = diza->opcode;
    } else {
        if (diza->p_66) *p++ = diza->p_66;
        if (diza->rex)  *p++ = diza->rex;
        *p++ = diza->opcode;
        if (diza->opcode == 0x0F) {
            *p++ = diza->opcode2;
            if (diza->opcode2 == 0x38 || diza->opcode2 == 0x3A)
                *p++ = diza->opcode3;
        }
    }

    if (diza->flag & C_MODRM) *p++ = diza->modrm;
    if (diza->flag & C_SIB)   *p++ = diza->sib;
    for (i = 0; i < diza->addrsize; i++) *p++ = diza->addr_b[i];
    for (i = 0; i < diza->datasize; i++) *p++ = diza->data_b[i];

    return (int)(p - opcode);
}
