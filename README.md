# NDISWDG - NDIS 2.0.1 pass-through ("wedge") module for OS/2 and ArcaOS

**Version 2.00**

NDISWDG is a small NDIS 2.0.1 "MAC-layer entity". It binds below to a real MAC driver and
presents the same MAC interface above to a protocol (for example TCP/IP). Everything that
crosses it is passed on unchanged, so the protocol and the MAC behave as if they were bound
directly. It is a starting point for NDIS filters, monitors and test tools, and a working,
documented example of the NDIS module interface (registration, binding, request, transmit and
receive paths).

This is a new, original implementation written from the public NDIS 2.0.1 specification.
It replaces the earlier assembler "wedge" sample, which came from a vendor DDK and could not
be published as open source. See `doc\PROVENANCE.md`.

## Status
* Builds clean with Open Watcom 2 (`bin\ndiswdg.os2`, a 16-bit NE device driver, about 5 KB, and the status tool `bin\wdgstat.exe`).
* **Works on the ArcaOS Dev VM:** loads, registers, binds to the NIC driver and passes TCP/IP traffic
  (an ssh session ran through it). See "Tested on" and `doc\DESIGN.md`.

## Tested on
* Build: ArcaOS Dev VM (VirtualBox), Open Watcom 2.0 (wcc, wasm, wlink, wmake), 2026-11-13.
* Run: ArcaOS 5.1.1 Dev VM (VirtualBox, Intel PRO/1000 NIC driver MMLEM.OS2, TCP/IP): loaded without
  errors, bound to the MAC, TCP/IP bound on top, transmit and receive indications counted (2026-11-13).
  Not tested: ReceiveChain and TransferData paths (this NIC uses lookahead receive), other MACs, real hardware,
  heavy load, SMP-specific races.

## License
BSD 3-Clause, see `LICENSE`.

## Authors
Martin Iturbide (with Claude), 2026.

## Original source
New code. Interface knowledge comes only from the 3Com/Microsoft LAN Manager NDIS 2.0.1
specification (chapters 3 to 6, appendix A).

## Version and BLDLEVEL
Version 2.00 (see `src\version.h`). The module carries a BLDLEVEL signature (`src\bldlevel.lnk`):

    bldlevel bin\ndiswdg.os2

## Build tools
* Open Watcom 2.0: `wcc` (16-bit C), `wasm`, `wlink`, `wmake`
* `tee` (C:\usr\bin) for the build log

## Build
Run from the project root on ArcaOS:

    compile_wat.cmd          build, writes compile.log, result bin\ndiswdg.os2
    compile_wat.cmd clean    remove bin\

or directly: `wmake -f Makefile.wat`

Compile flags: small model, 286, no stack checks, `-zu` (SS is not DS in a device driver),
packed structures, no C library. Link: `system os2 dll`, protected mode, CODE segment IOPL,
DOSCALLS imports from `os2.lib`; `hdr.obj` is linked first (device header at offset 0 of the
data) and `tail.obj` last (end-of-resident markers).

## Install
1. Copy `ndiswdg.os2` next to your MAC drivers (for example `C:\IBMCOM\MACS`).
2. In CONFIG.SYS, load it **after** `PROTMAN.OS2` and after the MAC it wraps:

       DEVICE=C:\IBMCOM\MACS\NDISWDG.OS2 /V

3. In PROTOCOL.INI add a section for the wedge and bind the protocol to it instead of to the MAC
   (see `test\protocol.ini.sample`):

       [WEDGE_NIF]
          DriverName = NDISWDG$
          Bindings = MYMAC_NIF

       [TCPIP_NIF]
          Bindings = WEDGE_NIF

The section name (`WEDGE_NIF` above) is free; the module finds its section by
`DriverName = NDISWDG$`. Keep a backup of CONFIG.SYS and PROTOCOL.INI: a wrong binding takes the
network down.

## Parameters (DEVICE= line)
Follows the common driver convention:

| Option | Meaning |
|---|---|
| (none) | quiet: only errors are shown |
| `/V` | verbose: banner, registered module and binding |
| `/Q` | no messages at all (not even errors) |
| `/W` | wait for a key after the messages |
| `/AA` | audible alert (beep) when the driver is loaded |

Every line starts with `NDISWDG: ` and long messages are word-wrapped at 79 columns with the prefix
on each line. PROTOCOL.INI keywords: `DriverName` (required, `NDISWDG$`) and `Bindings` (required,
the MAC to wrap; only the first name is used).

## How it works
See `doc\DESIGN.md`. Short version: `src\init.c` registers the module with the Protocol Manager at
INIT; `src\ndis.c` answers InitiateBind (binds to the MAC) and Bind (accepts one protocol) and forwards
every primitive in both directions, swapping only the module IDs and data selectors.

## Sample / test
* `test\protocol.ini.sample` and `test\config.sys.sample` show the configuration.
* `sample\wdgstat.c` (built to `bin\wdgstat.exe`) reads the module's status through the generic IOCTL
  (category 0xA5, function 1): whether the MAC below and the protocol above are bound, the module IDs
  and 13 traffic counters (each forwarded primitive in both directions, plus calls refused while not bound).
  Run `wdgstat` after the network is up; counters rising means traffic goes through the wedge.

## Known issues
* Counters are plain 32-bit increments without locking; a count can be off by one under a race.
* Static module only: dynamic bind/unbind (InitiatePrebind, InitiateUnbind, Unbind) answer INVALID_FUNCTION.
* One protocol per wedge (like a MAC). The Protocol Manager's VECTOR handles several protocols on one MAC.

## Changelog
* 2.00 (2026-11-13) First release of the new implementation, tested on the Dev VM.
