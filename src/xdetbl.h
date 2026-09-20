// Private opcode-attribute tables for XDE 2.00

#ifndef XDE_TBL_H
#define XDE_TBL_H

#include <stdint.h>
#include "xde.h"

#define XA_MODRM      0x00000001u
#define XA_REL        0x00000002u
#define XA_STOP       0x00000004u
#define XA_I64        0x00000008u
#define XA_O64        0x00000010u
#define XA_F64        0x00000020u
#define XA_D64        0x00000040u
#define XA_OPSZ8      0x00000080u
#define XA_UNDEF      0x00000100u
#define XA_BAD        0x00000200u
#define XA_INVALID    0x00000400u
#define XA_MOFFS      0x00000800u
#define XA_GROUP      0x00001000u
#define XA_3DNOW      0x00002000u
#define XA_VVVV_GPR   0x00004000u
#define XA_PUSH       0x00008000u
#define XA_POP        0x00010000u
#define XA_CALL       0x00020000u
#define XA_JMP        0x00040000u
#define XA_JCC        0x00080000u
#define XA_RET        0x00100000u

#define XA_IMM_SHIFT  21
#define XA_IMM_MASK   (0xFu << XA_IMM_SHIFT)
#define XA_IMM_NONE   0u
#define XA_IMM_IB     (1u << XA_IMM_SHIFT)
#define XA_IMM_IW     (2u << XA_IMM_SHIFT)
#define XA_IMM_IZ     (3u << XA_IMM_SHIFT)
#define XA_IMM_IV     (4u << XA_IMM_SHIFT)
#define XA_IMM_AP     (5u << XA_IMM_SHIFT)
#define XA_IMM_ENTER  (6u << XA_IMM_SHIFT)
#define XA_IMM_ID     (7u << XA_IMM_SHIFT)

#define XA_GRP_SHIFT  25
#define XA_GRP_MASK   (0x7Fu << XA_GRP_SHIFT)
#define XA_GRP(n)     (XA_GROUP | ((uint32_t)(n) << XA_GRP_SHIFT))
#define XA_GRP_ID(a)  (((a) & XA_GRP_MASK) >> XA_GRP_SHIFT)

enum xde_group_id {
    XG_NONE = 0,
    XG_1,
    XG_1A,
    XG_2,
    XG_3_1,
    XG_3_2,
    XG_4,
    XG_5,
    XG_6,
    XG_7,
    XG_8,
    XG_9,
    XG_10,
    XG_11A,
    XG_11B,
    XG_12,
    XG_13,
    XG_14,
    XG_15,
    XG_16,
    XG_17,
    XG_18,
    XG_19,
    XG_20,
    XG_21,
    XG_P,
    XG_XOP1,
    XG_XOP2,
    XG_XOP3,
    XG_XOP4,
    XG_COUNT
};

#define XDE_MAP_COUNT 11

extern const uint32_t xde_attr[XDE_MAP_COUNT][256];
extern const uint32_t xde_group[XG_COUNT][8];

#endif // XDE_TBL_H
