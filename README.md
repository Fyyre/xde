# XDE 2.00
# by Fyyre

eXtended disassembler engine for **x86, x86-64, VEX, EVEX, and XOP**.

This is a successor to z0mbie's XDE 1.02 (original sources in xde102)

Being a fan of XDE (and z0mbie) - I wanted it to work with new instruction sets.

It keeps the original split/merge model (`xde_disasm` / `xde_asm`) source/destination object sets, and extends them to 64-bit GPRs plus
AVX/AVX-512/XOP encodings.

## What it does

- Instruction **length** (capped at 15 bytes, Intel limit)
- Splits an instruction into `struct xde_instr` (prefixes, REX/VEX/EVEX/XOP,
  opcode map, ModR/M, SIB, displacement, immediate)
- Merges the structure back to bytes (`xde_asm`)
- Tracks **src_set / dst_set** bitmasks for GPRs, flags, memory, and I/O
  (SIMD/mask/control registers collapse to `XSET_OTHER`, same idea as 1.02)

## Layout

| Path | Purpose |
|------|---------|
| `xde102/` | Original XDE 1.02 sources |
| `include/xde.h` | Public API |
| `src/xde.c` | Decoder / encoder |
| `src/xdetbl.c` | Opcode attribute tables (generated) |
| `src/xde_text.c` | Flag / object-set printers |
| `tools/gen_tables.py` | Regenerates `xdetbl.c` |
| `tests/xde_test.c` | Length / encoding / round-trip tests |
| `msvc/xde.sln` | Visual Studio solution |

## Build (MSVC)

From a Developer Command Prompt, or just:

```
build.bat
```

It automatically locates vcvars64.bat, compiles with cl.exe, and runs build\xde_test.exe
If you rather use Visual Studio IDE open msvc\xde.sln (retarget the toolset if prompted).

Regenerating tables after editing tools/gen_tables.py:

```
python tools\gen_tables.py
```

## API

```c
#include "xde.h"

struct xde_instr diza;
int n = xde_disasm(ptr, &diza);				// 64-bit mode
n = xde_disasm_ex(ptr, &diza, XDE_MODE_32); // 16 / 32 / 64
n = xde_disasm_buf(ptr, max_len, &diza, XDE_MODE_64);
int m = xde_asm(out, &diza);
```

`n == 0` means the encoding is truncated, invalid in this mode, or undefined.

`diza.enc` is one of `XDE_ENC_LEGACY`, `XDE_ENC_VEX2`, `XDE_ENC_VEX3`,
`XDE_ENC_EVEX`, `XDE_ENC_XOP`, `XDE_ENC_REX2`

`diza.map` is the opcode map (`XDE_MAP_LEGACY`, `XDE_MAP_0F`, `XDE_MAP_0F38`,
`XDE_MAP_0F3A`, EVEX maps 4-6, VEX map 7, XOP maps 8/9/A)

Low 32 bits of `flag` / `src_set` / `dst_set` stay compatible with XDE 1.02
for EAX-EDI. RAX-RDI width bits, R8-R15, RIP, and encoding-class flags live
in the high half of the 64-bit fields.

## Notes

- 64-bit default address size is 8; `67` switches it to 4 (RIP-relative
  becomes abs32). Operand size is 4, `66` -> 2, `REX.W` / `VEX.W` -> 8.
- Near `CALL`/`JMP`/`Jcc` follow Intel **forced-64** behaviour: `66` does
  not shrink the rel32 displacement in 64-bit mode.
- `C4`/`C5`/`62`/`8F` are VEX/EVEX/XOP only when the following bytes match
  the prefix form; otherwise they remain LES/LDS/BOUND/POP in 16/32-bit mode.
- APX **REX2** (`D5` in 64-bit) is decoded so `D5` is not mistaken for AAD.

Original XDE 1.02 is 32-bit only and marks most SSE `0F` opcodes as errors.
This tree is the version to use for x86-64 and SIMD encodings.
