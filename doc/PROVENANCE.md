# Provenance

## What this is
NDISWDG 2.00 is new source code. It replaces an older sample ("DWB Generic Model NDIS Wedge driver",
1989-1991, written in MASM) that was distributed in a vendor DDK and therefore could not be released
as open source.

## Sources used for the new code
* The 3Com/Microsoft LAN Manager *Network Driver Interface Specification, Version 2.0.1 (Final)*:
  chapter 3 (interface), chapter 4 (data structures), chapter 5 (primitives), chapter 6 (Protocol
  Manager, OS/2 calling convention, static binding), appendix A (return codes).
* General knowledge of the OS/2 16-bit device driver interface (device header, strategy entry, INIT
  request packet) and of the Open Watcom tools.

No source code, include files or comments from the DDK samples, the old wedge sample or any other
driver were copied. The language (C with small assembler stubs instead of assembler only), the
structure (a pass-through of both dispatch tables instead of a test/IOCTL tool), the names and all text
are different.

## Honest limitation
The author who wrote this version (an AI assistant working with the project owner) had read the old
sample while checking its license, before the rewrite. Because of that this is a rewrite from the
specification, not a strict clean-room implementation. The code has no common text or structure with the
sample, but if a formal clean-room guarantee is needed, have the code reviewed by someone who has not
seen the old sample, or re-implement it from `doc\DESIGN.md` and the specification by a person or session
that never saw the old code.

## Not part of this repository
This repository was started fresh and has no history in common with the old sample. The old sample
stays only in its own repository (DEV-SAMPLE-DRV-wedge, tag `upstream-original`) and is not included
here, not even in the git history.
