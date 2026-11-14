# Changes

## 2.00 (2026-11-13), tested on the Dev VM
After the first boot tests: placeholder service-specific tables, loader-fixed entry point table (`_fptab`),
status IOCTL and counters, `sample\wdgstat.c`.

First release:
First release of the new implementation (see PROVENANCE.md). Replaces the old assembler wedge sample
completely: new source layout (`src\hdr.asm`, `tail.asm`, `init.c`, `ndis.c`, `ndis.h`, `version.h`),
new module name (`NDISWDG`, device name `NDISWDG$`), Open Watcom 2 build (`Makefile.wat`,
`compile_wat.cmd`), BSD 3-Clause license, BLDLEVEL signature, boot options `/V /Q /W /AA`.
