# Changes from the original (upstream-original)

Original: https://github.com/OS2World/DEV-SAMPLE-DRV-wedge, DWB Associates, version 1.05.
Goal: build it with Open Watcom 2 (wasm, wlink, wmake) on ArcaOS, following
`Plan - Driver Updates.txt`. No logic was changed on purpose; every source edit below only
adapts MASM 5.x syntax that WASM rejects.

## Version 1.06 (2026-11-13)
* Version raised from 1.05 to 1.06: `src\BUILD.CFG` (`MINOR2_VERSION_STR "6"`, `MINOR_VERSION 06`);
  the boot banner and the NDIS module version bytes now say 1.06.
* BLDLEVEL signature added: `src\bldlevel.lnk` (`option description '@#OS2World:1.06#@##1## ...'`),
  included by `Makefile.wat`. Verified with `BLDLEVEL.EXE bin\wedge.os2` (vendor OS2World, revision 1.06).
  The build date in the signature is the release date (fixed text, not the build time).
* License set to BSD 3-Clause (`LICENSE`); the original README said "Not Specified".
  The copyright lines name Martin Iturbide for the 2026 changes and DWB Associates for the original code.
* `bin\wedge.os2` is now 7,018 bytes (the description text was added).

## Assembler choice
The sources are MASM 5.x syntax (`MASM /MX`, `%Out`, `If1`, computed constants, macro
redefinition). Rewriting about 200 KB of 16-bit assembly for NASM was not practical, so the
Open Watcom assembler `wasm` (MASM compatible) is used, as allowed by the plan. NASM is not used.

## Structure
* All sources (`*.ASM`, `*.INC`, `BUILD.CFG`, `WEDGE.DEF`) moved to `src\` (git mv, history kept).
* Old build files `MAKEFILE`, `WEDGEOS2.LNK`, `WEDGEDOS.LNK` moved to `doc\original\` (reference only).
* New: `Makefile.wat`, `compile_wat.cmd`, `README.md`, `LICENSE`, `.gitignore`, `doc\CHANGES.md`.
* DOS target dropped (OS/2 only); `BUILDOS2` is defined by `Makefile.wat`.

## Source edits needed for WASM
| File | Original | Change | Why |
|---|---|---|---|
| src\BUILD.CFG | `If1` / `%Out ...` / `Endif` | `%Out` lines turned into comments, `If1` removed; the "invalid target" `%Out` became `.err <...>` | WASM has no `If1`/`%Out` |
| src\SSTRUCS.INC (CcName pad) | `DB NAME_LEN - ($ - CcName) dup(0)` | `DB NAME_LEN - 7 dup(0)` | WASM needs a constant count; 7 = length of `'MAC_NIF'` (NIC_Name) |
| src\SSTRUCS.INC (MscType pad) | `DB NAME_LEN-($-MscType) dup(0)` | `DB NAME_LEN-9 dup(0)` | 9 = length of `'DIX+802.3'` (MAC_Type) |
| src\WEDGE.INC | `NAME_LEN equ 16` | commented out | already defined (same value) in SSTRUCS.INC; WASM rejects equ redefinition |
| src\SMACROS.INC | `p86 Equ (@Cpu AND 01H)` | `p86 Equ 0` | `@Cpu` is not a constant in WASM; the build is 80286 (`-2p`), so "not 8086" |
| src\SMACROS.INC, src\DEVHLP.INC | `disableIntr` / `restoreIntr` macros defined in both files | guarded with `IFNDEF INTR_MACROS_DEFINED` | WASM rejects macro redefinition (identical macros) |
| src\IOCTL.INC, src\OS2EQU.INC | `STERR`, `STDON`, `STECODE`, `UNK_CMD` defined in both | each wrapped in `IFNDEF` | duplicate equates (same values) |
| src\OS2EQU.INC | `TRUE = 1`, `FALSE = 0` | commented out | already defined in BUILD.CFG |
| src\OS2EQU.INC | `DB (TYPE x) DUP (0)` for packet structs | `DB (SIZEOF x) DUP (0)` | WASM: size of a structure |
| src\SYSREQ.ASM | `dw (Not (Type SystemRequest))+1 Dup(?)` and `RetSysReqSize equ size SysReq - (2 + -(Type SystemRequest * 2))` | `dw 2 Dup(?)` and `size SysReq - 6` | `Type` of a FAR proc is -2 (far return address = 2 words); values computed by hand |
| src\SDATA.ASM | `PI_Count Equ ($ - StartPI) / Size PI_Record` (public, used by sndiinit.asm) | `PI_Count Equ 1` | WASM exported 0 for a computed public constant (warning W576); the table has exactly one `PI_Record` entry. This one would have broken the keyword loop in SNDIINIT |
| src\SDATA.ASM | `GDT_table equ $` (public) | `GDT_table label word` | same W576 problem (symbol would be 0) |
| src\DEVIOCTL.ASM | `ioctl_table equ $`, `command_table equ $` (public) | `label word` | same W576 problem |

## Build result
`compile_wat.cmd` builds all ten modules and links `bin\wedge.os2` (16-bit NE, protected mode,
module name WEDGE, 2 segments, imports DOSOPEN/DOSDEVIOCTL/DOSCLOSE/DOSWRITE from DOSCALLS).
Remaining warning: `sstrat.asm(264) W095 size not specified - BYTE PTR assumed` for
`cmp es:[bx].GIOCategory, SupCatagory`; `GIOCategory` is defined as `BYTE PTR PktData`, so the
assumed size is the intended one.

## Open points
* No MASM and no original binary were available, so the new binary could not be compared with
  a build of the original source. The by-hand constants above (7, 9, 2, 6, 1) were derived
  from the source and should be re-checked if the string names or tables change.
* Not run-tested (needs an NDIS MAC driver and PROTOCOL.INI).
* License: BSD 3-Clause was chosen for this release; no written permission from DWB Associates
  (the original copyright holder, "All Rights Reserved") was found. Confirm it before wide publication.
