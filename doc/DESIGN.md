# NDISWDG design

## Role
A MAC-layer entity as defined in chapter 6 of the NDIS 2.0.1 specification: protocol level and
interface type 1 (MAC) at both boundaries, binding supported at both boundaries (module flags 3).

    protocol (TCP/IP, ...)          calls our upper dispatch table, gets our lower-dispatch calls
            |
        NDISWDG  (this module)
            |
    real MAC driver                 we call its upper dispatch table, it calls our lower dispatch table

## Files
| File | Purpose |
|---|---|
| `src\hdr.asm` | device header (name `NDISWDG$`), strategy entry stub, `_dgroup_sel` (loader-resolved data selector) |
| `src\tail.asm` | `_end_of_code` / `_end_of_data` markers (linked last) |
| `src\init.c` | strategy, INIT: options, PROTOCOL.INI parsing, table setup, RegisterModule |
| `src\ndis.c` | system requests (InitiateBind, Bind) and all pass-through entry points |
| `src\ndis.h` | NDIS 2.0.1 structures, return codes, FAR PASCAL function types |
| `src\version.h`, `src\bldlevel.lnk` | version and BLDLEVEL signature |
| `sample\wdgstat.c` | status tool: reads the status IOCTL of the running module |

## Start-up (INIT, ring 3)
1. Parse the `DEVICE=` options (`/V /Q /W /AA`).
2. `DosOpen("\DEV\PROTMAN$")`, then the Protocol Manager IOCTL (category 0x81, function 0x58) with a
   request block: GetProtocolManagerInfo (opcode 1) returns the PROTOCOL.INI memory image.
3. Walk the image: sections, then keyword entries, then parameters (chapter 4 "Configuration Memory
   Image"). The section with `DRIVERNAME = NDISWDG$` is ours; its name becomes the module name and the
   first `BINDINGS` parameter is the MAC to wrap.
4. Fill the common characteristics table, the upper dispatch table and the lower dispatch table.
   The specification wants Ring 0 selectors, but this code runs at ring 3, so nothing may be built from the
   running CS or DS. The data selector comes from `_dgroup_sel` and the 13 entry points from `_fptab`
   (both in `hdr.asm`, written as assembler references), so the loader fixes them up. The tables also point
   to valid placeholder MAC service-specific characteristics and status tables: the Protocol Manager reads
   them while building its VECTOR structures and trapped on NULL pointers (see "Lessons").
5. RegisterModule (opcode 2) with a BindingsList containing the MAC name. The Protocol Manager writes our
   module ID into the table.
6. Report the end of resident code/data. On any error nothing stays resident.

## Binding (run time, ring 0)
The Protocol Manager binds from the bottom up and calls our system request entry (`SysReq`):
* **InitiateBind** (opcode 1, parameter = the MAC's table): we call the MAC's system request with Bind
  (opcode 2), passing our table and a slot for the MAC's table. We copy the MAC's module ID, data
  selector and the six upper-dispatch entry points into local data (the specification forbids using
  them before the bind succeeded) and take over the MAC's service-specific characteristics and status
  pointers, so the protocol sees the real MAC's description (station address, frame size, flags).
* **Bind** (opcode 2, from the protocol): refused with DRIVER_NOT_INITIALIZED until the MAC is bound;
  then we copy the protocol's module ID, data selector and six lower-dispatch entry points, and return
  our table. A second bind is refused (like a MAC).
* Other opcodes (prebind, unbind) return INVALID_FUNCTION: the module is static.

## Pass-through (run time, interrupt or task context)
All entry points are `__far __pascal __loadds` (callee removes the parameters, DS reloaded, last
parameter ignored). They never block and use almost no stack.

| Direction | Entry point | Forwarded to |
|---|---|---|
| protocol -> MAC | Request, TransmitChain, TransferData, ReceiveRelease, IndicationOn, IndicationOff | the MAC's upper dispatch table, our module ID as protocol ID, the MAC's data selector |
| MAC -> protocol | RequestConfirm, TransmitConfirm, ReceiveLookahead, IndicationComplete, ReceiveChain, Status | the protocol's lower dispatch table, our module ID as MAC ID, the protocol's data selector |

Request handles and buffer descriptors are passed unchanged, so no handle mapping or copying is needed.
If the module is not bound yet: requests fail with GENERAL_FAILURE, indications return
FRAME_NOT_RECOGNIZED (receive) or SUCCESS (confirm, complete, status).

## Where to add behaviour
Each forwarder in `src\ndis.c` is the place to count, filter, log or modify a primitive. Keep the rules of
chapter 5: do not block, do not change buffer descriptors, return the specified codes.

## Status interface
The device header allows open/close and generic IOCTL. Category 0xA5, function 1 returns a `WDGSTATUS`
structure (`src\ndis.h`): size, version, flags (bit 0 MAC bound, bit 1 protocol bound), module IDs, name and 13
counters. `wdgstat.exe` prints it. The counters are incremented in the forwarders (`COUNT()` in `ndis.c`).

## Lessons from the first tests
* Registering with NULL service-specific characteristics/status pointers made PROTMAN trap (TRAP 000D, ES = 0,
  null selector). Provide valid placeholder tables and replace the pointers at bind time.
* Far pointers built by C code at INIT time (`g_cct.sysReq = SysReq;`) use the Ring 3 CS of the running code.
  Use assembler references (`dd SYSREQ`) so the loader supplies the Ring 0 selector. `wlink` marks the segments
  movable and the loader resolves such references through the entry table; that works for this driver.
* Do not keep helper files in `bin\`: `compile_wat.cmd clean` empties it.

## Tests done on the Dev VM (2026-11-13)
Hardware/driver: VirtualBox, Intel PRO/1000 Legacy NIC with `MMLEM.OS2`, TCP/IP (`TCPIP$`), ArcaOS 5.1.1.
1. Driver loaded without a PROTOCOL.INI section: printed the error and unloaded cleanly. OK.
2. Section `[WEDGE_NIF]` with `Bindings = MMLEM_NIF`, TCP/IP still on the NIC: after the two fixes above,
   registered and bound to the MAC (`wdgstat`: MAC bound yes, protocol bound no, counters 0). OK.
3. `[TCPIP_NIF]` `Bindings = WEDGE_NIF,RTL88EU_NIF`: TCP/IP bound to the wedge and used the network (ssh session
   over it); TransmitChain, TransmitConfirm, ReceiveLookahead and IndicationComplete counters rose. OK.
Not yet exercised: ReceiveChain, TransferData, ReceiveRelease, Status indications, another MAC, real hardware.

## First test plan (kept for reference)
On the ArcaOS Dev VM (NIC driver `MMLEM.OS2`, protocol TCP/IP):
1. Add `DEVICE=...\NDISWDG.OS2 /V` after the MAC in CONFIG.SYS, with no `[WEDGE_NIF]` section: the driver
   must print the "no section with DRIVERNAME" error and unload (tests INIT, PROTMAN access, image parsing).
2. Add the section with `Bindings = MMLEM_NIF`, protocol still bound to the MAC: registration and
   InitiateBind through the Protocol Manager's VECTOR.
3. Bind the protocol to the wedge: full data path (ping, ssh).
Back up CONFIG.SYS and PROTOCOL.INI first; a wrong binding stops the network.
