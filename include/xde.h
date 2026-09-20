// XDE v2.00 by Fyyre - eXtended disassembler engine (x86 / x86-64 / VEX / EVEX / XOP)

// Successor to z0mbie's XDE 1.02: instruction length, split/merge, and source/destination object sets.
// Original 1.02 sources present in xde102.


#ifndef XDE_H
#define XDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4201) // nameless union (also C11)
#endif

#define XDE_VERSION_MAJOR 2
#define XDE_VERSION_MINOR 0
#define XDE_VERSION       ((XDE_VERSION_MAJOR << 8) | XDE_VERSION_MINOR)
#define XDE_MAXLEN        15

// Execution / decode mode (bits)
#define XDE_MODE_16  16
#define XDE_MODE_32  32
#define XDE_MODE_64  64

// Encoding class (xde_instr.enc)
#define XDE_ENC_LEGACY  0
#define XDE_ENC_VEX2    1
#define XDE_ENC_VEX3    2
#define XDE_ENC_EVEX    3
#define XDE_ENC_XOP     4
#define XDE_ENC_REX2    5

// Opcode maps (xde_instr.map)
#define XDE_MAP_LEGACY  0
#define XDE_MAP_0F      1
#define XDE_MAP_0F38    2
#define XDE_MAP_0F3A    3
#define XDE_MAP_EVEX4   4
#define XDE_MAP_EVEX5   5
#define XDE_MAP_EVEX6   6
#define XDE_MAP_VEX7    7
#define XDE_MAP_XOP8    8
#define XDE_MAP_XOP9    9
#define XDE_MAP_XOPA    10

// Instruction flags (xde_instr.flag) - low 32 bits compatible with XDE 1.02
#define C_SPECIAL  0
#define C_ADDR1    0x00000001u
#define C_ADDR2    0x00000002u
#define C_ADDR4    0x00000004u
#define C_MODRM    0x00000008u
#define C_SIB      0x00000010u
#define C_ADDR67   0x00000020u
#define C_DATA66   0x00000040u
#define C_UNDEF    0x00000080u
#define C_DATA1    0x00000100u
#define C_DATA2    0x00000200u
#define C_DATA4    0x00000400u
#define C_BAD      0x00000800u
#define C_REL      0x00001000u
#define C_STOP     0x00002000u
#define C_OPSZ8    0x00004000u
#define C_SRC_FL   0x00008000u
#define C_DST_FL   0x00010000u
#define C_MOD_FL   (C_SRC_FL+C_DST_FL)
#define C_SRC_REG  0x00020000u
#define C_SRC_RM   0x00040000u
#define C_DST_REG  0x00080000u
#define C_DST_RM   0x00100000u
#define C_MOD_REG  (C_SRC_REG+C_DST_REG)
#define C_MOD_RM   (C_SRC_RM+C_DST_RM)
#define C_SRC_ACC  0x00200000u
#define C_DST_ACC  0x00400000u
#define C_MOD_ACC  (C_SRC_ACC+C_DST_ACC)
#define C_SRC_R0   0x00800000u
#define C_DST_R0   0x01000000u
#define C_MOD_R0   (C_SRC_R0+C_DST_R0)
#define C_PUSH     0x02000000u
#define C_POP      0x04000000u
#define C_x_shift  27
#define C_x_00001  0x08000000u
#define C_x_00010  0x10000000u
#define C_x_00100  0x20000000u
#define C_x_01000  0x40000000u
#define C_x_10000  0x80000000u
#define C_x_mask   0xF8000000u
#define C_ERROR    0xFFFFFFFFu

#define XDE_CMD(fl)  ((uint32_t)(fl) & C_x_mask)
#define C_CMD_other  ( 0u << C_x_shift)
#define C_CMD_CALL   ( 1u << C_x_shift)
#define C_CMD_JMP    ( 2u << C_x_shift)
#define C_CMD_JCC    ( 3u << C_x_shift)
#define C_CMD_RET    ( 4u << C_x_shift)

// Extended flags in the high 32 bits of xde_instr.flag
#define C_DATA8    0x0000000100000000ULL  // 8-byte immediate (MOV r64, imm64)
#define C_ADDR8    0x0000000200000000ULL  // 8-byte moffs
#define C_RIPREL   0x0000000400000000ULL  // RIP-relative ModR/M
#define C_REX      0x0000000800000000ULL
#define C_VEX      0x0000001000000000ULL
#define C_EVEX     0x0000002000000000ULL
#define C_XOP      0x0000004000000000ULL
#define C_REX2     0x0000008000000000ULL
#define C_I64      0x0000010000000000ULL  // invalid in 64-bit mode
#define C_O64      0x0000020000000000ULL  // 64-bit mode only
#define C_F64      0x0000040000000000ULL  // forced-64 (Intel ignores 0x66 on Jz)
#define C_D64      0x0000080000000000ULL  // default-64 operand (PUSH/POP)
#define C_3DNOW    0x0000100000000000ULL

// Object sets (src_set / dst_set). Low 32 bits match XDE 1.02
#define XSET_AL    0x00000001ULL
#define XSET_AH    0x00000002ULL
#define XSET_AX    0x00000003ULL
#define XSET_EAX   0x0000000FULL
#define XSET_CL    0x00000010ULL
#define XSET_CH    0x00000020ULL
#define XSET_CX    0x00000030ULL
#define XSET_ECX   0x000000F0ULL
#define XSET_DL    0x00000100ULL
#define XSET_DH    0x00000200ULL
#define XSET_DX    0x00000300ULL
#define XSET_EDX   0x00000F00ULL
#define XSET_BL    0x00001000ULL
#define XSET_BH    0x00002000ULL
#define XSET_BX    0x00003000ULL
#define XSET_EBX   0x0000F000ULL
#define XSET_SP    0x00010000ULL
#define XSET_ESP   0x00030000ULL
#define XSET_BP    0x00100000ULL
#define XSET_EBP   0x00300000ULL
#define XSET_SI    0x01000000ULL
#define XSET_ESI   0x03000000ULL
#define XSET_DI    0x10000000ULL
#define XSET_EDI   0x30000000ULL
#define XSET_ALL16 0x11113333ULL
#define XSET_ALL32 0x3333FFFFULL
#define XSET_FL    0x00040000ULL
#define XSET_MEM   0x00080000ULL
#define XSET_OTHER 0x00400000ULL   // seg / FPU / MMX / XMM / YMM / ZMM / CR / DR / K
#define XSET_DEV   0x00800000ULL

// 64-bit width of RAX..RDI (in addition to the 32-bit XSET_E* masks)
#define XSET_RAX   (XSET_EAX | 0x0000010000000000ULL)
#define XSET_RCX   (XSET_ECX | 0x0000020000000000ULL)
#define XSET_RDX   (XSET_EDX | 0x0000040000000000ULL)
#define XSET_RBX   (XSET_EBX | 0x0000080000000000ULL)
#define XSET_RSP   (XSET_ESP | 0x0000100000000000ULL)
#define XSET_RBP   (XSET_EBP | 0x0000200000000000ULL)
#define XSET_RSI   (XSET_ESI | 0x0000400000000000ULL)
#define XSET_RDI   (XSET_EDI | 0x0000800000000000ULL)
#define XSET_ALL64 (XSET_RAX|XSET_RCX|XSET_RDX|XSET_RBX|XSET_RSP|XSET_RBP|XSET_RSI|XSET_RDI)

// R8..R15 (any width)
#define XSET_R8    0x0000000100000000ULL
#define XSET_R9    0x0000000200000000ULL
#define XSET_R10   0x0000000400000000ULL
#define XSET_R11   0x0000000800000000ULL
#define XSET_R12   0x0000001000000000ULL
#define XSET_R13   0x0000002000000000ULL
#define XSET_R14   0x0000004000000000ULL
#define XSET_R15   0x0000008000000000ULL
#define XSET_R8_15 (XSET_R8|XSET_R9|XSET_R10|XSET_R11|XSET_R12|XSET_R13|XSET_R14|XSET_R15)

#define XSET_RIP   0x0001000000000000ULL
#define XSET_UNDEF 0xFFFFFFFFFFFFFFFFULL


struct xde_instr
{
    uint8_t  mode;           // XDE_MODE_16 / 32 / 64
    uint8_t  defaddr;        // 2, 4 or 8
    uint8_t  defdata;        // 2, 4 or 8
    uint8_t  len;            // total length, 1..15
    uint8_t  addrsize;       // displacement / moffs size
    uint8_t  datasize;       // immediate size
    uint8_t  enc;            // XDE_ENC_*
    uint8_t  map;            // XDE_MAP_*

    uint64_t flag;           // C_* flags
    uint64_t src_set;
    uint64_t dst_set;

    uint8_t  p_lock;         // 0 or 0xF0
    uint8_t  p_66;           // 0 or 0x66 (legacy; not used with VEX/EVEX/XOP)
    uint8_t  p_67;           // 0 or 0x67
    uint8_t  p_rep;          // 0 or 0xF2/0xF3
    uint8_t  p_seg;          // 0 or 26/2E/36/3E/64/65
    uint8_t  rex;            // 0 or 40..4F
    uint8_t  nvex;           // 0, 2, 3 or 4 (bytes in vex[])
    uint8_t  vex[4];

    uint8_t  opcode;         // first opcode byte (0x0F if two-byte escape)
    uint8_t  opcode2;        // second opcode (0F xx or VEX/EVEX/XOP opcode)
    uint8_t  opcode3;        // third opcode (0F 38/3A xx)
    uint8_t  modrm;
    uint8_t  sib;

    uint8_t  rex_w, rex_r, rex_x, rex_b;
    uint8_t  vex_pp;         // 0=none, 1=66, 2=F3, 3=F2
    uint8_t  vex_l;          // L or L'L
    uint8_t  vex_vvvv;       // decoded (non-inverted) vvvv, 0..31
    uint8_t  evex_z;
    uint8_t  evex_b;
    uint8_t  evex_aaa;
    uint8_t  evex_r2;        // EVEX.R'

    union {
        uint8_t  addr_b[8];
        uint16_t addr_w[4];
        uint32_t addr_d[2];
        uint64_t addr_q[1];
        int8_t   addr_c[8];
        int16_t  addr_s[4];
        int32_t  addr_l[2];
        int64_t  addr_q64[1];
    };
    union {
        uint8_t  data_b[8];
        uint16_t data_w[4];
        uint32_t data_d[2];
        uint64_t data_q[1];
        int8_t   data_c[8];
        int16_t  data_s[4];
        int32_t  data_l[2];
        int64_t  data_q64[1];
    };
};

// Decode. Returns instruction length, or 0 on error.
// xde_disasm() uses 64-bit mode.
// xde_disasm_ex() uses mode (16/32/64).
// xde_disasm_buf() also bounds the read to max_len bytes (capped at 15).
int __cdecl xde_disasm(const uint8_t *opcode, struct xde_instr *diza);
int __cdecl xde_disasm_ex(const uint8_t *opcode, struct xde_instr *diza, unsigned mode);
int __cdecl xde_disasm_buf(const uint8_t *opcode, unsigned max_len, struct xde_instr *diza, unsigned mode);

// Encode from a filled xde_instr. Returns bytes written.
int __cdecl xde_asm(uint8_t *opcode, const struct xde_instr *diza);

// Debug printers (optional). output should be at least 256 bytes.
void __cdecl xde_sprintfl(char *output, uint64_t fl);
void __cdecl xde_sprintset(char *output, uint64_t set);

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#ifdef __cplusplus
}
#endif

#endif // XDE_H
