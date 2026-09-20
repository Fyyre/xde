// XDE 2.00 self-test: length, encoding class, and rt assembly.

#include "xde.h"

#include <stdio.h>
#include <string.h>

static int g_fail;

static void fail(const char *name, const char *msg)
{
    printf("FAIL %s: %s\n", name, msg);
    g_fail++;
}

static void hexbytes(char *out, const uint8_t *b, int n)
{
    int i;
    out[0] = 0;
    for (i = 0; i < n; i++) {
        char tmp[8];
        sprintf(tmp, "%s%02X", i ? " " : "", b[i]);
        strcat(out, tmp);
    }
}

static void expect_len(const char *name, unsigned mode,
                       const uint8_t *b, unsigned n, int want)
{
    struct xde_instr d;
    int got = xde_disasm_buf(b, n, &d, mode);
    char hx[128];
    hexbytes(hx, b, (int)n);
    if (got != want) {
        char msg[256];
        sprintf(msg, "len=%d want=%d bytes=%s", got, want, hx);
        fail(name, msg);
        return;
    }
    if (got > 0 && d.len != (uint8_t)got) {
        fail(name, "diza.len mismatch");
        return;
    }
    printf("ok %-28s %s len=%d\n", name, hx, got);
}

static void expect_enc(const char *name, unsigned mode,
                       const uint8_t *b, unsigned n, int want_len, int enc)
{
    struct xde_instr d;
    int got = xde_disasm_buf(b, 15, &d, mode);
    if (got != want_len) {
        char msg[128];
        sprintf(msg, "len=%d want=%d", got, want_len);
        fail(name, msg);
        return;
    }
    if (d.enc != (uint8_t)enc) {
        char msg[128];
        sprintf(msg, "enc=%u want=%d", d.enc, enc);
        fail(name, msg);
        return;
    }
    printf("ok %-28s enc=%d len=%d\n", name, enc, got);
}

static void expect_fail(const char *name, unsigned mode, const uint8_t *b, unsigned n)
{
    struct xde_instr d;
    int got = xde_disasm_buf(b, n, &d, mode);
    if (got != 0) {
        char msg[128];
        sprintf(msg, "expected 0, got %d", got);
        fail(name, msg);
        return;
    }
    printf("ok %-28s rejected\n", name);
}

static void expect_roundtrip(const char *name, unsigned mode, const uint8_t *b, unsigned n)
{
    struct xde_instr d;
    uint8_t out[16];
    int got, asz, got2;
    struct xde_instr d2;

    got = xde_disasm_buf(b, 15, &d, mode);
    if (got != (int)n) {
        char msg[128];
        sprintf(msg, "disasm len=%d want=%u", got, n);
        fail(name, msg);
        return;
    }
    asz = xde_asm(out, &d);
    if (asz != got) {
        char msg[128];
        sprintf(msg, "asm len=%d want=%d", asz, got);
        fail(name, msg);
        return;
    }
    got2 = xde_disasm_buf(out, 15, &d2, mode);
    if (got2 != got || d2.opcode != d.opcode || d2.modrm != d.modrm) {
        fail(name, "re-disasm mismatch");
        return;
    }
    printf("ok %-28s rt %d\n", name, got);
}

int main(void)
{
    // 64-bit GP
    {
        static const uint8_t nop[] = { 0x90 };
        expect_len("nop", 64, nop, 1, 1);
    }
    {
        static const uint8_t xor_eax[] = { 0x31, 0xC0 };
        expect_len("xor eax,eax", 64, xor_eax, 2, 2);
    }
    {
        static const uint8_t xor_rax[] = { 0x48, 0x31, 0xC0 };
        expect_len("xor rax,rax", 64, xor_rax, 3, 3);
        expect_roundtrip("xor rax,rax rt", 64, xor_rax, 3);
    }
    {
        static const uint8_t mov_imm64[] = {
            0x48, 0xB8, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
        };
        expect_len("mov rax,imm64", 64, mov_imm64, 10, 10);
    }
    {
        static const uint8_t mov_eax[] = { 0xB8, 0x01, 0x02, 0x03, 0x04 };
        expect_len("mov eax,imm32", 64, mov_eax, 5, 5);
    }
    {
        static const uint8_t mov_r8[] = { 0x49, 0xB8, 1,2,3,4,5,6,7,8 };
        expect_len("mov r8,imm64", 64, mov_r8, 10, 10);
    }
    {
        static const uint8_t rip[] = { 0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00 };
        expect_len("mov rax,[rip+0]", 64, rip, 7, 7);
        {
            struct xde_instr d;
            xde_disasm(rip, &d);
            if (!(d.flag & C_RIPREL))
                fail("mov rax,[rip+0]", "missing C_RIPREL");
            else
                printf("ok %-28s RIPREL\n", "mov rax,[rip+0] flag");
        }
    }
    {
        static const uint8_t callm[] = { 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00 };
        expect_len("call [rip+0]", 64, callm, 6, 6);
    }
    {
        static const uint8_t call[] = { 0xE8, 0x00, 0x00, 0x00, 0x00 };
        expect_len("call rel32", 64, call, 5, 5);
    }
    {
        static const uint8_t call66[] = { 0x66, 0xE8, 0x00, 0x00, 0x00, 0x00 };
        expect_len("66 call rel32 (Intel f64)", 64, call66, 6, 6);
    }
    {
        static const uint8_t ret[] = { 0xC3 };
        expect_len("ret", 64, ret, 1, 1);
    }
    {
        static const uint8_t push[] = { 0x55 };
        expect_len("push rbp", 64, push, 1, 1);
    }
    {
        static const uint8_t sub[] = { 0x48, 0x83, 0xEC, 0x20 };
        expect_len("sub rsp,0x20", 64, sub, 4, 4);
    }
    {
        static const uint8_t sib[] = { 0x89, 0x4C, 0x24, 0x08 };
        expect_len("mov [rsp+8],ecx", 64, sib, 4, 4);
    }
    {
        static const uint8_t r8[] = { 0x4C, 0x8B, 0x44, 0x24, 0x28 };
        expect_len("mov r8,[rsp+0x28]", 64, r8, 5, 5);
    }
    {
        static const uint8_t longnop[] = { 0x0F, 0x1F, 0x44, 0x00, 0x00 };
        expect_len("nop dword [rax+rax]", 64, longnop, 5, 5);
    }
    {
        static const uint8_t endbr[] = { 0xF3, 0x0F, 0x1E, 0xFA };
        expect_len("endbr64", 64, endbr, 4, 4);
    }
    {
        static const uint8_t sys[] = { 0x0F, 0x05 };
        expect_len("syscall", 64, sys, 2, 2);
    }
    {
        static const uint8_t movsxd[] = { 0x48, 0x63, 0xC3 };
        expect_len("movsxd rax,ebx", 64, movsxd, 3, 3);
    }
    {
        static const uint8_t moffs[] = {
            0xA1, 1,2,3,4,5,6,7,8
        };
        expect_len("mov eax,[moffs64]", 64, moffs, 9, 9);
    }
    {
        static const uint8_t rex_moffs[] = {
            0x48, 0xA1, 1,2,3,4,5,6,7,8
        };
        expect_len("mov rax,[moffs64]", 64, rex_moffs, 10, 10);
    }
    {
        static const uint8_t testf7[] = { 0x48, 0xF7, 0xC0, 0xFF, 0x00, 0x00, 0x00 };
        expect_len("test rax,imm32", 64, testf7, 7, 7);
    }
    {
        static const uint8_t bt[] = { 0x48, 0x0F, 0xBA, 0xE0, 0x01 };
        expect_len("bt rax,1", 64, bt, 5, 5);
    }
    {
        static const uint8_t a67[] = { 0x67, 0x8B, 0x00 };
        expect_len("67 mov eax,[eax]", 64, a67, 3, 3);
    }

    // SSE / 0F38 / 0F3A
    {
        static const uint8_t movups[] = { 0x0F, 0x10, 0x00 };
        expect_len("movups xmm0,[rax]", 64, movups, 3, 3);
    }
    {
        static const uint8_t palignr[] = { 0x66, 0x0F, 0x3A, 0x0F, 0xC0, 0x01 };
        expect_len("palignr xmm0,xmm0,1", 64, palignr, 6, 6);
    }
    {
        static const uint8_t pshufb[] = { 0x66, 0x0F, 0x38, 0x00, 0xC1 };
        expect_len("pshufb xmm0,xmm1", 64, pshufb, 5, 5);
    }
    {
        static const uint8_t crc[] = { 0xF2, 0x0F, 0x38, 0xF0, 0xC1 };
        expect_len("crc32 eax,cl", 64, crc, 5, 5);
    }
    {
        static const uint8_t movbe[] = { 0x0F, 0x38, 0xF0, 0x00 };
        expect_len("movbe eax,[rax]", 64, movbe, 4, 4);
    }
    {
        static const uint8_t now[] = { 0x0F, 0x0F, 0xC1, 0xBF };
        expect_len("pavgusb mm0,mm1 (3DNow)", 64, now, 4, 4);
    }

    // VEX
    {
        static const uint8_t vaddps[] = { 0xC5, 0xF8, 0x58, 0xC1 };
        expect_enc("vaddps xmm0,xmm0,xmm1", 64, vaddps, 4, 4, XDE_ENC_VEX2);
        expect_roundtrip("vaddps rt", 64, vaddps, 4);
    }
    {
        static const uint8_t vandn[] = { 0xC4, 0xE2, 0x78, 0xF2, 0xC1 };
        expect_enc("andn eax,eax,ecx", 64, vandn, 5, 5, XDE_ENC_VEX3);
    }
    {
        static const uint8_t vandn64[] = { 0xC4, 0xE2, 0xF8, 0xF2, 0xC1 };
        expect_enc("andn rax,rax,rcx", 64, vandn64, 5, 5, XDE_ENC_VEX3);
    }
    {
        static const uint8_t rorx[] = { 0xC4, 0xE3, 0xFB, 0xF0, 0xC1, 0x03 };
        expect_enc("rorx rax,rcx,3", 64, rorx, 6, 6, XDE_ENC_VEX3);
    }
    {
        static const uint8_t vzeroupper[] = { 0xC5, 0xF8, 0x77 };
        expect_enc("vzeroupper", 64, vzeroupper, 3, 3, XDE_ENC_VEX2);
    }

    // EVEX
    {
        static const uint8_t evadd[] = { 0x62, 0xF1, 0x7C, 0x48, 0x58, 0xC1 };
        expect_enc("vaddps zmm0,zmm0,zmm1", 64, evadd, 6, 6, XDE_ENC_EVEX);
        expect_roundtrip("evex vaddps rt", 64, evadd, 6);
    }
    {
        static const uint8_t evmem[] = {
            0x62, 0xF1, 0x7C, 0x48, 0x58, 0x05, 0x00, 0x00, 0x00, 0x00
        };
        expect_enc("vaddps zmm0,zmm0,[rip]", 64, evmem, 10, 10, XDE_ENC_EVEX);
    }
    {
        static const uint8_t evib[] = {
            0x62, 0xF3, 0x7D, 0x48, 0x0A, 0xC1, 0x01
        };
        expect_enc("vrndscaless xmm0,xmm0,xmm1,1", 64, evib, 7, 7, XDE_ENC_EVEX);
    }

    // XOP
    {
        static const uint8_t vfrcz[] = { 0x8F, 0xE9, 0x78, 0x81, 0xC1 };
        expect_enc("vfrczpd xmm0,xmm1", 64, vfrcz, 5, 5, XDE_ENC_XOP);
        expect_roundtrip("xop vfrczpd rt", 64, vfrcz, 5);
    }
    {
        static const uint8_t vpcom[] = { 0x8F, 0xE8, 0x78, 0xCC, 0xC1, 0x00 };
        expect_enc("vpcomb xmm0,xmm0,xmm1,0", 64, vpcom, 6, 6, XDE_ENC_XOP);
    }
    {
        static const uint8_t bextr[] = { 0x8F, 0xEA, 0x78, 0x10, 0xC1, 0x01, 0x00, 0x00, 0x00 };
        expect_enc("bextr eax,ecx,imm32", 64, bextr, 9, 9, XDE_ENC_XOP);
    }

    // 32-bit mode: LES vs VEX, BOUND vs EVEX
    {
        static const uint8_t les[] = { 0xC4, 0x00 };
        expect_len("les eax,[eax] (32)", 32, les, 2, 2);
    }
    {
        static const uint8_t vex32[] = { 0xC5, 0xF8, 0x58, 0xC0 };
        expect_enc("vaddps (32-bit VEX2)", 32, vex32, 4, 4, XDE_ENC_VEX2);
    }
    {
        static const uint8_t bound[] = { 0x62, 0x00 };
        expect_len("bound eax,[eax] (32)", 32, bound, 2, 2);
    }
    {
        static const uint8_t inc[] = { 0x40 };
        expect_len("inc eax (32)", 32, inc, 1, 1);
        expect_fail("truncated REX in 64", 64, inc, 1);
    }
    {
        static const uint8_t rex_nop[] = { 0x40, 0x90 };
        expect_len("rex nop", 64, rex_nop, 2, 2);
    }
    {
        static const uint8_t aaa[] = { 0x37 };
        expect_fail("aaa invalid in 64", 64, aaa, 1);
        expect_len("aaa in 32", 32, aaa, 1, 1);
    }
    {
        static const uint8_t push_es[] = { 0x06 };
        expect_fail("push es invalid in 64", 64, push_es, 1);
    }

    // REX2 (APX)
    {
        static const uint8_t rex2[] = { 0xD5, 0x40, 0x8D, 0x00 };
        expect_enc("rex2 lea r16d,[rax]", 64, rex2, 4, 4, XDE_ENC_REX2);
    }
    {
        static const uint8_t rex2m[] = { 0xD5, 0xC0, 0xAF, 0xC0 };
        expect_enc("rex2 imul r16d,eax", 64, rex2m, 4, 4, XDE_ENC_REX2);
    }

    {
        static const uint8_t pop[] = { 0x8F, 0xC0 };
        expect_len("pop rax (8F /0)", 64, pop, 2, 2);
    }
    {
        static const uint8_t test8[] = { 0xF6, 0xC0, 0x12 };
        expect_len("test al,0x12", 64, test8, 3, 3);
    }
    {
        static const uint8_t notal[] = { 0xF6, 0xD0 };
        expect_len("not al", 64, notal, 2, 2);
    }
    {
        static const uint8_t callreg[] = { 0xFF, 0xD0 };
        expect_len("call rax", 64, callreg, 2, 2);
    }

    // 16-bit
    {
        static const uint8_t add16[] = { 0x01, 0xC0 };
        expect_len("add ax,ax (16)", 16, add16, 2, 2);
    }
    {
        static const uint8_t movoff[] = { 0xA1, 0x00, 0x10 };
        expect_len("mov ax,[moffs16]", 16, movoff, 3, 3);
    }

    // truncated
    {
        static const uint8_t cut[] = { 0x48, 0xB8, 0x01 };
        expect_fail("truncated mov rax,imm64", 64, cut, 3);
    }

    printf("\n%d failure(s)\n", g_fail);
    return g_fail ? 1 : 0;
}
