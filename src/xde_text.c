// XDE v2.00 - flag + object-set printers

#include "xde.h"

#include <string.h>

void __cdecl xde_sprintfl(char *output, uint64_t fl)
{
    output[0] = 0;
    if (fl & C_BAD)    strcat(output, "C_BAD|");
    if (fl & C_REL)    strcat(output, "C_REL|");
    if (fl & C_STOP)   strcat(output, "C_STOP|");
    if (fl & C_MODRM)  strcat(output, "C_MODRM|");
    if (fl & C_SIB)    strcat(output, "C_SIB|");
    if (fl & C_RIPREL) strcat(output, "C_RIPREL|");
    if (fl & C_REX)    strcat(output, "C_REX|");
    if (fl & C_VEX)    strcat(output, "C_VEX|");
    if (fl & C_EVEX)   strcat(output, "C_EVEX|");
    if (fl & C_XOP)    strcat(output, "C_XOP|");
    if (fl & C_REX2)   strcat(output, "C_REX2|");
    if (fl & C_UNDEF)  strcat(output, "C_UNDEF|");
    if (output[0] && output[strlen(output) - 1] == '|')
        output[strlen(output) - 1] = 0;
}

void __cdecl xde_sprintset(char *output, uint64_t set)
{
    output[0] = 0;

    if (set == XSET_UNDEF) {
        strcat(output, "???");
        return;
    }

    if ((set & XSET_RAX) == XSET_RAX) strcat(output, "RAX|");
    else if ((set & XSET_EAX) == XSET_EAX) strcat(output, "EAX|");
    else if ((set & XSET_AX) == XSET_AX) strcat(output, "AX|");
    else {
        if (set & XSET_AL) strcat(output, "AL|");
        if (set & XSET_AH) strcat(output, "AH|");
    }

    if ((set & XSET_RCX) == XSET_RCX) strcat(output, "RCX|");
    else if ((set & XSET_ECX) == XSET_ECX) strcat(output, "ECX|");
    else if ((set & XSET_CX) == XSET_CX) strcat(output, "CX|");
    else {
        if (set & XSET_CL) strcat(output, "CL|");
        if (set & XSET_CH) strcat(output, "CH|");
    }

    if ((set & XSET_RDX) == XSET_RDX) strcat(output, "RDX|");
    else if ((set & XSET_EDX) == XSET_EDX) strcat(output, "EDX|");
    else if ((set & XSET_DX) == XSET_DX) strcat(output, "DX|");
    else {
        if (set & XSET_DL) strcat(output, "DL|");
        if (set & XSET_DH) strcat(output, "DH|");
    }

    if ((set & XSET_RBX) == XSET_RBX) strcat(output, "RBX|");
    else if ((set & XSET_EBX) == XSET_EBX) strcat(output, "EBX|");
    else if ((set & XSET_BX) == XSET_BX) strcat(output, "BX|");
    else {
        if (set & XSET_BL) strcat(output, "BL|");
        if (set & XSET_BH) strcat(output, "BH|");
    }

    if ((set & XSET_RSP) == XSET_RSP) strcat(output, "RSP|");
    else if ((set & XSET_ESP) == XSET_ESP) strcat(output, "ESP|");
    else if (set & XSET_SP) strcat(output, "SP|");

    if ((set & XSET_RBP) == XSET_RBP) strcat(output, "RBP|");
    else if ((set & XSET_EBP) == XSET_EBP) strcat(output, "EBP|");
    else if (set & XSET_BP) strcat(output, "BP|");

    if ((set & XSET_RSI) == XSET_RSI) strcat(output, "RSI|");
    else if ((set & XSET_ESI) == XSET_ESI) strcat(output, "ESI|");
    else if (set & XSET_SI) strcat(output, "SI|");

    if ((set & XSET_RDI) == XSET_RDI) strcat(output, "RDI|");
    else if ((set & XSET_EDI) == XSET_EDI) strcat(output, "EDI|");
    else if (set & XSET_DI) strcat(output, "DI|");

    if (set & XSET_R8)  strcat(output, "R8|");
    if (set & XSET_R9)  strcat(output, "R9|");
    if (set & XSET_R10) strcat(output, "R10|");
    if (set & XSET_R11) strcat(output, "R11|");
    if (set & XSET_R12) strcat(output, "R12|");
    if (set & XSET_R13) strcat(output, "R13|");
    if (set & XSET_R14) strcat(output, "R14|");
    if (set & XSET_R15) strcat(output, "R15|");

    if (set & XSET_RIP)   strcat(output, "RIP|");
    if (set & XSET_FL)    strcat(output, "F|");
    if (set & XSET_MEM)   strcat(output, "M|");
    if (set & XSET_OTHER) strcat(output, "other|");
    if (set & XSET_DEV)   strcat(output, "dev|");

    if (output[0] && output[strlen(output) - 1] == '|')
        output[strlen(output) - 1] = 0;
}
