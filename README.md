# DEV-SAMPLE-DRV-NET-wedge

**Version 1.06**

DWB Associates Generic Model NDIS "Wedge" driver (sample). It sits between an NDIS protocol
stack and a MAC driver and is used as a driver example. Updated to build with Open Watcom 2
on ArcaOS / OS/2 Warp 4.52.

## Status
**Builds** with the Open Watcom 2 tools (`bin\wedge.os2`, a 16-bit NE protected-mode module).
**Not run-tested**: it needs an NDIS MAC driver and a configuration (PROTOCOL.INI) to load,
and no original binary or MASM was available to compare the result byte by byte.
See `doc\CHANGES.md` for every change from the original source and the open points.

## Tested on
* Build: ArcaOS Dev VM (VirtualBox), Open Watcom 2.0 (wasm 2.0.1, wlink, wmake). 2026-11-13.
* Run: not tested.

## License
IBM DDK License Based

## Authors
* DWB Associates (original code, 1989-1991)
* Open Watcom build update and version 1.06: Martin Iturbide (with Claude), 2026

## Original source
https://github.com/OS2World/DEV-SAMPLE-DRV-wedge (version 1.05, dated 1992-01-22 in the history).
The unmodified state is tagged `upstream-original`; the original MASM makefile and linker
files are kept in `doc\original\`.

## Version and BLDLEVEL
Version 1.06 (original 1.05). The module carries a BLDLEVEL signature (`src\bldlevel.lnk`):

    bldlevel bin\wedge.os2
    Vendor:        OS2World
    Revision:      1.06
    Description:   DWB Generic Model NDIS Wedge driver (Open Watcom build)

The boot banner of the module (`src\smsgs.asm`) also prints "1.06". The version is set in
`src\BUILD.CFG` (MINOR2_VERSION_STR / MINOR_VERSION) and must be kept in sync with
`src\bldlevel.lnk`.

## Build tools
* Open Watcom 2.0: `wasm` (MASM compatible assembler), `wlink`, `wmake`
  (the plan prefers NASM; the sources are MASM syntax, so WASM was used - see `doc\CHANGES.md`)
* `tee` (C:\usr\bin) for the log

## Build
Run from the project root on ArcaOS:

    compile_wat.cmd          build, writes compile.log, result bin\wedge.os2
    compile_wat.cmd clean    remove bin\

or directly: `wmake -f Makefile.wat`

Link options (`Makefile.wat`): `system os2 dll`, `option protmode`, `option caseexact`,
`option nodefaultlibs`, import library `C:\os2\doscalls.lib`, same object order as the old
`WEDGEOS2.LNK`.

## Install
Not verified (the module was not run). It is an NDIS module (`wedge.os2`), not a CONFIG.SYS
`DEVICE=` driver: it would be copied to the NDIS drivers directory and declared in
PROTOCOL.INI of the NDIS stack. For that reason the boot parameters of the driver standard
(`/V /Q /W /AA`) do not apply.

## Parameters
None. Configuration comes from the PROTOCOL.INI keywords handled in `src\sndiinit.asm`.

## Sample / test
None yet.

## Known issues
* DOS target (`BUILDDOS`, old `WEDGEDOS.LNK`) is no longer built - OS/2 only.
* Not run-tested (see Status).

## Changelog
* 1.06 (2026-11-13)  Open Watcom 2 build (wasm/wlink/wmake), restructured to the standard layout,
  BSD 3-Clause license, BLDLEVEL signature, version raised from 1.05 to 1.06.
