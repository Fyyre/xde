#!/usr/bin/env python3
"""Generate src/xdetbl.c from Intel/AMD opcode maps (length-relevant attributes)."""
from __future__ import annotations

import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "src", "xdetbl.c")

XA_MODRM = 0x00000001
XA_REL = 0x00000002
XA_STOP = 0x00000004
XA_I64 = 0x00000008
XA_O64 = 0x00000010
XA_F64 = 0x00000020
XA_D64 = 0x00000040
XA_OPSZ8 = 0x00000080
XA_UNDEF = 0x00000100
XA_BAD = 0x00000200
XA_INVALID = 0x00000400
XA_MOFFS = 0x00000800
XA_GROUP = 0x00001000
XA_3DNOW = 0x00002000
XA_VVVV_GPR = 0x00004000
XA_PUSH = 0x00008000
XA_POP = 0x00010000
XA_CALL = 0x00020000
XA_JMP = 0x00040000
XA_JCC = 0x00080000
XA_RET = 0x00100000
XA_IMM_SHIFT = 21
XA_GRP_SHIFT = 25

IMM_NONE = 0
IMM_IB = 1 << XA_IMM_SHIFT
IMM_IW = 2 << XA_IMM_SHIFT
IMM_IZ = 3 << XA_IMM_SHIFT
IMM_IV = 4 << XA_IMM_SHIFT
IMM_AP = 5 << XA_IMM_SHIFT
IMM_ENTER = 6 << XA_IMM_SHIFT
IMM_ID = 7 << XA_IMM_SHIFT


def GRP(n: int) -> int:
    return XA_GROUP | (n << XA_GRP_SHIFT)


XG_1, XG_1A, XG_2, XG_3_1, XG_3_2 = 1, 2, 3, 4, 5
XG_4, XG_5, XG_6, XG_7, XG_8 = 6, 7, 8, 9, 10
XG_9, XG_10, XG_11A, XG_11B = 11, 12, 13, 14
XG_12, XG_13, XG_14, XG_15 = 15, 16, 17, 18
XG_16, XG_17, XG_18, XG_19 = 19, 20, 21, 22
XG_20, XG_21, XG_P = 23, 24, 25
XG_XOP1, XG_XOP2, XG_XOP3, XG_XOP4 = 26, 27, 28, 29
XG_COUNT = 30


def blank(fill: int = XA_INVALID) -> list[int]:
    return [fill] * 256


def put(m: list[int], keys, val: int) -> None:
    if isinstance(keys, int):
        m[keys] = val
        return
    for k in keys:
        m[k] = val


def rng(a: int, b: int) -> range:
    return range(a, b + 1)


# Map 0: one-byte opcodes
m0 = blank(0)

# ALU Eb/Ev/Gb/Gv
for base in (0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38):
    m0[base + 0] = XA_MODRM | XA_OPSZ8
    m0[base + 1] = XA_MODRM
    m0[base + 2] = XA_MODRM | XA_OPSZ8
    m0[base + 3] = XA_MODRM
    m0[base + 4] = IMM_IB | XA_OPSZ8
    m0[base + 5] = IMM_IZ

# PUSH/POP ES/CS/SS/DS
m0[0x06] = XA_PUSH | XA_I64 | XA_BAD
m0[0x07] = XA_POP | XA_I64 | XA_BAD
m0[0x0E] = XA_PUSH | XA_I64 | XA_BAD
m0[0x16] = XA_PUSH | XA_I64 | XA_BAD
m0[0x17] = XA_POP | XA_I64 | XA_BAD
m0[0x1E] = XA_PUSH | XA_I64 | XA_BAD
m0[0x1F] = XA_POP | XA_I64 | XA_BAD

# 0x0F is an escape - decoder consumes it before lookup
m0[0x0F] = 0

# prefixes: should not be looked up. mark as invalid-if-reached
for p in (0x26, 0x2E, 0x36, 0x3E, 0x64, 0x65, 0x66, 0x67, 0xF0, 0xF2, 0xF3):
    m0[p] = XA_INVALID

m0[0x27] = XA_I64 | XA_BAD  # DAA
m0[0x2F] = XA_I64 | XA_BAD  # DAS
m0[0x37] = XA_I64 | XA_BAD  # AAA
m0[0x3F] = XA_I64 | XA_BAD  # AAS

# INC/DEC eAX.. - i64 (REX in 64-bit is consumed as prefix)
for i in rng(0x40, 0x4F):
    m0[i] = XA_I64

# PUSH/POP r64
for i in rng(0x50, 0x57):
    m0[i] = XA_PUSH | XA_D64
for i in rng(0x58, 0x5F):
    m0[i] = XA_POP | XA_D64

m0[0x60] = XA_PUSH | XA_I64 | XA_BAD  # PUSHA
m0[0x61] = XA_POP | XA_I64 | XA_BAD  # POPA
m0[0x62] = XA_MODRM | XA_I64 | XA_BAD | XA_UNDEF  # BOUND
m0[0x63] = XA_MODRM  # ARPL / MOVSXD

m0[0x68] = IMM_IZ | XA_PUSH | XA_D64
m0[0x69] = XA_MODRM | IMM_IZ
m0[0x6A] = IMM_IB | XA_PUSH | XA_D64
m0[0x6B] = XA_MODRM | IMM_IB
m0[0x6C] = XA_BAD  # INS
m0[0x6D] = XA_BAD
m0[0x6E] = XA_BAD  # OUTS
m0[0x6F] = XA_BAD

for i in rng(0x70, 0x7F):
    fl = IMM_IB | XA_REL | XA_F64 | XA_JCC | XA_UNDEF
    if i in (0x70, 0x71, 0x7A, 0x7B):
        fl |= XA_BAD
    m0[i] = fl

m0[0x80] = XA_MODRM | IMM_IB | GRP(XG_1) | XA_OPSZ8
m0[0x81] = XA_MODRM | IMM_IZ | GRP(XG_1)
m0[0x82] = XA_MODRM | IMM_IB | GRP(XG_1) | XA_OPSZ8 | XA_I64 | XA_BAD
m0[0x83] = XA_MODRM | IMM_IB | GRP(XG_1)
m0[0x84] = XA_MODRM | XA_OPSZ8  # TEST
m0[0x85] = XA_MODRM
m0[0x86] = XA_MODRM | XA_OPSZ8  # XCHG
m0[0x87] = XA_MODRM
m0[0x88] = XA_MODRM | XA_OPSZ8  # MOV
m0[0x89] = XA_MODRM
m0[0x8A] = XA_MODRM | XA_OPSZ8
m0[0x8B] = XA_MODRM
m0[0x8C] = XA_MODRM | XA_BAD  # MOV sreg
m0[0x8D] = XA_MODRM  # LEA
m0[0x8E] = XA_MODRM | XA_BAD
m0[0x8F] = XA_MODRM | XA_POP | XA_D64 | GRP(XG_1A)  # POP Ev / XOP prefix handled earlier

m0[0x90] = 0  # NOP / XCHG
for i in rng(0x91, 0x97):
    m0[i] = 0
m0[0x98] = 0  # CBW/CWDE/CDQE
m0[0x99] = 0  # CWD/CDQ/CQO
m0[0x9A] = IMM_AP | XA_I64 | XA_BAD | XA_UNDEF | XA_CALL
m0[0x9B] = XA_UNDEF  # WAIT
m0[0x9C] = XA_PUSH | XA_D64 | XA_BAD  # PUSHF
m0[0x9D] = XA_POP | XA_D64 | XA_BAD  # POPF
m0[0x9E] = XA_BAD  # SAHF
m0[0x9F] = XA_BAD  # LAHF

m0[0xA0] = XA_MOFFS | XA_OPSZ8
m0[0xA1] = XA_MOFFS
m0[0xA2] = XA_MOFFS | XA_OPSZ8
m0[0xA3] = XA_MOFFS
m0[0xA4] = 0  # MOVS
m0[0xA5] = 0
m0[0xA6] = 0  # CMPS
m0[0xA7] = 0
m0[0xA8] = IMM_IB | XA_OPSZ8
m0[0xA9] = IMM_IZ
m0[0xAA] = XA_OPSZ8
m0[0xAB] = 0
m0[0xAC] = XA_OPSZ8
m0[0xAD] = XA_BAD
m0[0xAE] = XA_OPSZ8
m0[0xAF] = XA_BAD

for i in rng(0xB0, 0xB7):
    m0[i] = IMM_IB | XA_OPSZ8
for i in rng(0xB8, 0xBF):
    m0[i] = IMM_IV

m0[0xC0] = XA_MODRM | IMM_IB | GRP(XG_2) | XA_OPSZ8
m0[0xC1] = XA_MODRM | IMM_IB | GRP(XG_2)
m0[0xC2] = IMM_IW | XA_STOP | XA_RET | XA_UNDEF
m0[0xC3] = XA_STOP | XA_RET | XA_UNDEF
m0[0xC4] = XA_MODRM | XA_I64 | XA_BAD  # LES / VEX
m0[0xC5] = XA_MODRM | XA_I64 | XA_BAD  # LDS / VEX
m0[0xC6] = XA_MODRM | IMM_IB | GRP(XG_11A) | XA_OPSZ8
m0[0xC7] = XA_MODRM | IMM_IZ | GRP(XG_11B)
m0[0xC8] = IMM_ENTER
m0[0xC9] = XA_D64  # LEAVE
m0[0xCA] = IMM_IW | XA_STOP | XA_RET | XA_BAD | XA_UNDEF
m0[0xCB] = XA_STOP | XA_RET | XA_BAD | XA_UNDEF
m0[0xCC] = XA_BAD  # INT3
m0[0xCD] = IMM_IB | XA_UNDEF
m0[0xCE] = XA_I64 | XA_BAD | XA_UNDEF  # INTO
m0[0xCF] = XA_STOP | XA_RET | XA_BAD | XA_UNDEF  # IRET

m0[0xD0] = XA_MODRM | GRP(XG_2) | XA_OPSZ8
m0[0xD1] = XA_MODRM | GRP(XG_2)
m0[0xD2] = XA_MODRM | GRP(XG_2) | XA_OPSZ8
m0[0xD3] = XA_MODRM | GRP(XG_2)
m0[0xD4] = IMM_IB | XA_I64 | XA_BAD  # AAM
m0[0xD5] = IMM_IB | XA_I64 | XA_BAD  # AAD / REX2 in 64-bit
m0[0xD6] = XA_BAD | XA_OPSZ8  # SALC
m0[0xD7] = XA_BAD | XA_OPSZ8  # XLAT
for i in rng(0xD8, 0xDF):
    m0[i] = XA_MODRM | XA_UNDEF  # x87

for i in (0xE0, 0xE1):
    m0[i] = IMM_IB | XA_REL | XA_F64 | XA_UNDEF | XA_BAD
m0[0xE2] = IMM_IB | XA_REL | XA_F64 | XA_UNDEF
m0[0xE3] = IMM_IB | XA_REL | XA_F64 | XA_UNDEF
m0[0xE4] = IMM_IB | XA_OPSZ8 | XA_BAD
m0[0xE5] = IMM_IB | XA_BAD
m0[0xE6] = IMM_IB | XA_OPSZ8 | XA_BAD
m0[0xE7] = IMM_IB | XA_BAD
m0[0xE8] = IMM_IZ | XA_REL | XA_F64 | XA_CALL | XA_UNDEF
m0[0xE9] = IMM_IZ | XA_REL | XA_F64 | XA_JMP | XA_STOP | XA_UNDEF
m0[0xEA] = IMM_AP | XA_I64 | XA_JMP | XA_STOP | XA_BAD | XA_UNDEF
m0[0xEB] = IMM_IB | XA_REL | XA_F64 | XA_JMP | XA_STOP | XA_UNDEF
m0[0xEC] = XA_OPSZ8 | XA_BAD
m0[0xED] = XA_BAD
m0[0xEE] = XA_OPSZ8 | XA_BAD
m0[0xEF] = XA_BAD

m0[0xF1] = XA_BAD | XA_UNDEF  # INT1
m0[0xF4] = XA_BAD  # HLT
m0[0xF5] = XA_BAD
m0[0xF6] = XA_MODRM | GRP(XG_3_1) | XA_OPSZ8
m0[0xF7] = XA_MODRM | GRP(XG_3_2)
m0[0xF8] = 0
m0[0xF9] = 0
m0[0xFA] = XA_BAD
m0[0xFB] = XA_BAD
m0[0xFC] = 0
m0[0xFD] = 0
m0[0xFE] = XA_MODRM | GRP(XG_4) | XA_OPSZ8
m0[0xFF] = XA_MODRM | GRP(XG_5)


# Map 1: two-byte 0F
m1 = blank(XA_INVALID)
m1[0x00] = XA_MODRM | GRP(XG_6) | XA_UNDEF
m1[0x01] = XA_MODRM | GRP(XG_7) | XA_UNDEF
m1[0x02] = XA_MODRM | XA_UNDEF  # LAR
m1[0x03] = XA_MODRM | XA_UNDEF  # LSL
m1[0x05] = XA_O64  # SYSCALL
m1[0x06] = XA_UNDEF  # CLTS
m1[0x07] = XA_O64  # SYSRET
m1[0x08] = XA_UNDEF  # INVD
m1[0x09] = XA_UNDEF  # WBINVD
m1[0x0B] = XA_UNDEF  # UD2
m1[0x0D] = XA_MODRM | GRP(XG_P)  # prefetch
m1[0x0E] = 0  # FEMMS
m1[0x0F] = XA_MODRM | XA_3DNOW | IMM_IB  # 3DNow opcode suffix

# SSE/AVX 0F 10-1F : ModRM (NOP Ev at 1F)
for i in rng(0x10, 0x17):
    m1[i] = XA_MODRM
m1[0x18] = XA_MODRM | GRP(XG_16)
m1[0x19] = XA_MODRM  # NOP / reserved hint
m1[0x1A] = XA_MODRM  # MPX
m1[0x1B] = XA_MODRM
m1[0x1C] = XA_MODRM | GRP(XG_20)
m1[0x1D] = XA_MODRM
m1[0x1E] = XA_MODRM | GRP(XG_21)
m1[0x1F] = XA_MODRM  # NOP Ev

m1[0x20] = XA_MODRM  # MOV Rd, Cd
m1[0x21] = XA_MODRM
m1[0x22] = XA_MODRM
m1[0x23] = XA_MODRM

for i in rng(0x28, 0x2F):
    m1[i] = XA_MODRM

m1[0x30] = XA_UNDEF  # WRMSR
m1[0x31] = 0  # RDTSC
m1[0x32] = XA_UNDEF  # RDMSR
m1[0x33] = 0  # RDPMC
m1[0x34] = 0  # SYSENTER
m1[0x35] = 0  # SYSEXIT
m1[0x37] = 0  # GETSEC
# 38/3A are escapes handled by decoder
m1[0x38] = XA_INVALID
m1[0x3A] = XA_INVALID

for i in rng(0x40, 0x4F):
    m1[i] = XA_MODRM  # CMOVcc / k-reg

for i in rng(0x50, 0x6F):
    m1[i] = XA_MODRM

m1[0x70] = XA_MODRM | IMM_IB
m1[0x71] = XA_MODRM | IMM_IB | GRP(XG_12)
m1[0x72] = XA_MODRM | IMM_IB | GRP(XG_13)
m1[0x73] = XA_MODRM | IMM_IB | GRP(XG_14)
for i in rng(0x74, 0x76):
    m1[i] = XA_MODRM
m1[0x77] = 0  # EMMS / vzeroupper
for i in rng(0x78, 0x7F):
    m1[i] = XA_MODRM

for i in rng(0x80, 0x8F):
    m1[i] = IMM_IZ | XA_REL | XA_F64 | XA_JCC | XA_UNDEF

for i in rng(0x90, 0x9F):
    m1[i] = XA_MODRM | XA_OPSZ8 | XA_UNDEF  # SETcc / kmov

m1[0xA0] = XA_PUSH | XA_D64
m1[0xA1] = XA_POP | XA_D64
m1[0xA2] = 0  # CPUID
m1[0xA3] = XA_MODRM  # BT
m1[0xA4] = XA_MODRM | IMM_IB
m1[0xA5] = XA_MODRM
m1[0xA6] = XA_MODRM  # VIA / PadLock
m1[0xA7] = XA_MODRM
m1[0xA8] = XA_PUSH | XA_D64
m1[0xA9] = XA_POP | XA_D64
m1[0xAA] = XA_UNDEF  # RSM
m1[0xAB] = XA_MODRM
m1[0xAC] = XA_MODRM | IMM_IB
m1[0xAD] = XA_MODRM
m1[0xAE] = XA_MODRM | GRP(XG_15)
m1[0xAF] = XA_MODRM  # IMUL

m1[0xB0] = XA_MODRM | XA_OPSZ8
m1[0xB1] = XA_MODRM
m1[0xB2] = XA_MODRM | XA_BAD
m1[0xB3] = XA_MODRM
m1[0xB4] = XA_MODRM | XA_BAD
m1[0xB5] = XA_MODRM | XA_BAD
m1[0xB6] = XA_MODRM
m1[0xB7] = XA_MODRM
m1[0xB8] = XA_MODRM  # POPCNT / JMPE
m1[0xB9] = XA_MODRM | GRP(XG_10)  # UD1
m1[0xBA] = XA_MODRM | IMM_IB | GRP(XG_8)
m1[0xBB] = XA_MODRM
m1[0xBC] = XA_MODRM
m1[0xBD] = XA_MODRM
m1[0xBE] = XA_MODRM
m1[0xBF] = XA_MODRM

m1[0xC0] = XA_MODRM | XA_OPSZ8
m1[0xC1] = XA_MODRM
m1[0xC2] = XA_MODRM | IMM_IB  # cmpps
m1[0xC3] = XA_MODRM  # movnti
m1[0xC4] = XA_MODRM | IMM_IB
m1[0xC5] = XA_MODRM | IMM_IB
m1[0xC6] = XA_MODRM | IMM_IB
m1[0xC7] = XA_MODRM | GRP(XG_9)
for i in rng(0xC8, 0xCF):
    m1[i] = 0  # BSWAP

for i in rng(0xD0, 0xFE):
    m1[i] = XA_MODRM
m1[0xFF] = XA_MODRM  # UD0 Gv, Ev (Intel: ModRM)


# Map 2: 0F 38 - default ModRM; a few VEX.vvvv GPRs
m2 = blank(XA_INVALID)
_of38 = [
    *rng(0x00, 0x17),
    *rng(0x18, 0x1F),
    *rng(0x20, 0x2F),
    *rng(0x30, 0x47),
    0x49, 0x4B,
    *rng(0x4C, 0x4F),
    *rng(0x50, 0x55),
    *rng(0x58, 0x5C),
    0x5E,
    *rng(0x62, 0x66),
    0x68, 0x6C,
    *rng(0x70, 0x73),
    *rng(0x75, 0x7F),
    *rng(0x80, 0x83),
    *rng(0x88, 0x8F),
    *rng(0x90, 0x93),
    *rng(0x96, 0x9F),
    *rng(0xA0, 0xA3),
    *rng(0xA6, 0xAF),
    0xB0, 0xB1,
    *rng(0xB4, 0xBF),
    0xC4, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCF,
    0xD2, 0xD3, 0xD8, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    *rng(0xE0, 0xEF),
    0xF0, 0xF1, 0xF2, 0xF3, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC,
]
for i in _of38:
    m2[i] = XA_MODRM
m2[0xC6] = XA_MODRM | GRP(XG_18)
m2[0xC7] = XA_MODRM | GRP(XG_19)
# BMI / BMI2: vvvv is a GPR
for i in (0xF2, 0xF3, 0xF5, 0xF6, 0xF7):
    m2[i] |= XA_VVVV_GPR
m2[0xF3] = XA_MODRM | GRP(XG_17) | XA_VVVV_GPR


# Map 3: 0F 3A - ModRM + Ib almost everywhere
m3 = blank(XA_INVALID)
_of3a = [
    *rng(0x00, 0x06),
    *rng(0x08, 0x0F),
    *rng(0x14, 0x1B),
    0x1D, 0x1E, 0x1F,
    *rng(0x20, 0x23),
    0x25, 0x26, 0x27,
    *rng(0x30, 0x33),
    *rng(0x38, 0x3B),
    0x3E, 0x3F,
    *rng(0x40, 0x44),
    0x46, 0x4A, 0x4B, 0x4C,
    0x50, 0x51, 0x54, 0x55, 0x56, 0x57,
    *rng(0x60, 0x63),
    0x66, 0x67,
    *rng(0x70, 0x73),
    0xC2, 0xCC, 0xCE, 0xCF, 0xDE, 0xDF, 0xF0,
]
for i in _of3a:
    m3[i] = XA_MODRM | IMM_IB
m3[0xF0] |= XA_VVVV_GPR  # RORX


# Map 4: EVEX map 4 (APX / promoted integer)
m4 = blank(XA_INVALID)
for base in (0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38):
    m4[base + 0] = XA_MODRM | XA_OPSZ8
    m4[base + 1] = XA_MODRM
    m4[base + 2] = XA_MODRM | XA_OPSZ8
    m4[base + 3] = XA_MODRM
m4[0x24] = XA_MODRM | IMM_IB  # SHLD
m4[0x2C] = XA_MODRM | IMM_IB  # SHRD
for i in rng(0x40, 0x4F):
    m4[i] = XA_MODRM
m4[0x60] = XA_MODRM
m4[0x61] = XA_MODRM
m4[0x65] = XA_MODRM
m4[0x66] = XA_MODRM
m4[0x69] = XA_MODRM | IMM_IZ
m4[0x6B] = XA_MODRM | IMM_IB
m4[0x80] = XA_MODRM | IMM_IB | GRP(XG_1) | XA_OPSZ8
m4[0x81] = XA_MODRM | IMM_IZ | GRP(XG_1)
m4[0x83] = XA_MODRM | IMM_IB | GRP(XG_1)
m4[0x84] = XA_MODRM | XA_OPSZ8
m4[0x85] = XA_MODRM
m4[0x88] = XA_MODRM
m4[0x8F] = XA_MODRM | XA_POP
m4[0xA5] = XA_MODRM
m4[0xAD] = XA_MODRM
m4[0xAF] = XA_MODRM
m4[0xC0] = XA_MODRM | IMM_IB | GRP(XG_2) | XA_OPSZ8
m4[0xC1] = XA_MODRM | IMM_IB | GRP(XG_2)
m4[0xD0] = XA_MODRM | GRP(XG_2) | XA_OPSZ8
m4[0xD1] = XA_MODRM | GRP(XG_2)
m4[0xD2] = XA_MODRM | GRP(XG_2) | XA_OPSZ8
m4[0xD3] = XA_MODRM | GRP(XG_2)
m4[0xF0] = XA_MODRM
m4[0xF1] = XA_MODRM
m4[0xF2] = XA_MODRM
m4[0xF4] = XA_MODRM
m4[0xF5] = XA_MODRM
m4[0xF6] = XA_MODRM | GRP(XG_3_1) | XA_OPSZ8
m4[0xF7] = XA_MODRM | GRP(XG_3_2)
m4[0xF8] = XA_MODRM
m4[0xF9] = XA_MODRM
m4[0xFE] = XA_MODRM | GRP(XG_4) | XA_OPSZ8
m4[0xFF] = XA_MODRM | GRP(XG_5)


# Map 5 / 6: EVEX FP16 etc. - ModRM
m5 = blank(XA_INVALID)
for i in (
    0x10, 0x11, 0x1D, 0x2A, 0x2C, 0x2D, 0x2E, 0x2F,
    0x51, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x6E, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E,
):
    m5[i] = XA_MODRM

m6 = blank(XA_INVALID)
for i in (
    0x13, 0x2C, 0x2D, 0x42, 0x43, 0x4C, 0x4D, 0x4E, 0x4F,
    0x56, 0x57,
    *rng(0x96, 0x9F),
    *rng(0xA6, 0xAF),
    *rng(0xB6, 0xBF),
    0xD6, 0xD7,
):
    m6[i] = XA_MODRM


# Map 7: VEX map 7
m7 = blank(XA_INVALID)
m7[0xF8] = XA_MODRM | IMM_ID  # URDMSR / UWRMSR r, imm32


# XOP maps 8, 9, A
m8 = blank(XA_INVALID)
for i in (
    0x85, 0x86, 0x87, 0x8E, 0x8F,
    0x95, 0x96, 0x97, 0x9E, 0x9F,
    0xA2, 0xA3, 0xA6, 0xB6,
    0xC0, 0xC1, 0xC2, 0xC3,
    0xCC, 0xCD, 0xCE, 0xCF,
    0xEC, 0xED, 0xEE, 0xEF,
):
    m8[i] = XA_MODRM | IMM_IB  # Lo / Ib fourth operand

m9 = blank(XA_INVALID)
m9[0x01] = XA_MODRM | GRP(XG_XOP1) | XA_VVVV_GPR
m9[0x02] = XA_MODRM | GRP(XG_XOP2) | XA_VVVV_GPR
m9[0x12] = XA_MODRM | GRP(XG_XOP3)
for i in rng(0x80, 0x83):
    m9[i] = XA_MODRM
for i in rng(0x90, 0x9B):
    m9[i] = XA_MODRM | XA_VVVV_GPR
for i in (0xC1, 0xC2, 0xC3, 0xC6, 0xC7, 0xCB, 0xD1, 0xD2, 0xD3, 0xD6, 0xD7, 0xDB, 0xE1, 0xE2, 0xE3):
    m9[i] = XA_MODRM

ma = blank(XA_INVALID)
ma[0x10] = XA_MODRM | IMM_ID | XA_VVVV_GPR  # BEXTR Gy,Ey,Id
ma[0x12] = XA_MODRM | IMM_ID | GRP(XG_XOP4) | XA_VVVV_GPR

MAPS = [m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, ma]
assert all(len(m) == 256 for m in MAPS)


# Group extra attributes (OR'ed after /r is known). Immediate overrides.
group = [[0] * 8 for _ in range(XG_COUNT)]
# Grp3_1 TEST Eb,Ib on /0 and /1
group[XG_3_1][0] = IMM_IB
group[XG_3_1][1] = IMM_IB
group[XG_3_2][0] = IMM_IZ
group[XG_3_2][1] = IMM_IZ
# Grp5 CALL/JMP/PUSH
group[XG_5][2] = XA_CALL | XA_UNDEF
group[XG_5][3] = XA_CALL | XA_UNDEF
group[XG_5][4] = XA_JMP | XA_STOP | XA_UNDEF
group[XG_5][5] = XA_JMP | XA_STOP | XA_UNDEF
group[XG_5][6] = XA_PUSH
group[XG_5][7] = XA_BAD
# Grp4 only /0 /1 legal
for r in range(2, 8):
    group[XG_4][r] = XA_BAD
# Grp1A POP only /0
for r in range(1, 8):
    group[XG_1A][r] = XA_BAD
# Grp8 BT* /4..7 ; opcode already has Ib
for r in range(0, 4):
    group[XG_8][r] = XA_BAD
# Grp10 all UD1
for r in range(8):
    group[XG_10][r] = XA_BAD
# GrpXOP4 already has Id on opcode
# Grp12-14 already have Ib on opcode
# Grp17 BMI
for r in (1, 2, 3):
    group[XG_17][r] = XA_VVVV_GPR


def fmt_row(row: list[int]) -> str:
    parts = []
    for i, v in enumerate(row):
        comma = "," if i != 255 else ""
        parts.append(f"0x{v:08X}{comma}")
    lines = []
    for i in range(0, 256, 8):
        lines.append("        " + " ".join(parts[i : i + 8]))
    return "\n".join(lines)


map_names = [
    "legacy",
    "0F",
    "0F38",
    "0F3A",
    "EVEX.4",
    "EVEX.5",
    "EVEX.6",
    "VEX.7",
    "XOP.8",
    "XOP.9",
    "XOP.A",
]

lines = [
    "// Auto-generated by tools/gen_tables.py - do not edit by hand.",
    "",
    '#include "xdetbl.h"',
    "",
    "const uint32_t xde_attr[XDE_MAP_COUNT][256] = {",
]
for mi, m in enumerate(MAPS):
    comma = "," if mi + 1 != len(MAPS) else ""
    lines.append(f"    {{ // {map_names[mi]}")
    lines.append(fmt_row(m))
    lines.append(f"    }}{comma}")
lines.append("};")
lines.append("")
lines.append("const uint32_t xde_group[XG_COUNT][8] = {")
for gi in range(XG_COUNT):
    comma = "," if gi + 1 != XG_COUNT else ""
    vals = ", ".join(f"0x{v:08X}" for v in group[gi])
    lines.append(f"    {{ {vals} }}{comma}")
lines.append("};")
lines.append("")

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", newline="\n", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
print(f"wrote {OUT}")
