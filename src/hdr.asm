; hdr.asm - device header, strategy entry stub and segment layout of NDISWDG.OS2
;
; Copyright (c) 2026, Martin Iturbide
; All rights reserved. BSD 3-Clause License, see LICENSE.
;
; This object must be linked FIRST: the device header has to sit at offset 0 of the
; data group. The segments below use the same names and classes as the C compiler
; (small model), so the linker merges them with the C code and data.

        .286p

; entry points implemented in ndis.c (FAR PASCAL names are upper case)
        extrn   SYSREQ:far, WDGREQUEST:far, WDGTRANSMITCHAIN:far, WDGTRANSFERDATA:far
        extrn   WDGRECEIVERELEASE:far, WDGINDICATIONON:far, WDGINDICATIONOFF:far
        extrn   WDGREQUESTCONFIRM:far, WDGTRANSMITCONFIRM:far, WDGRECEIVELOOKAHEAD:far
        extrn   WDGINDICATIONCOMPLETE:far, WDGRECEIVECHAIN:far, WDGSTATUS:far

_DATA   segment word public 'DATA'
        public  _dgroup_sel

; ---- device header ------------------------------------------------------------
        dd      -1                      ; link to the next device header
        dw      89C0h                   ; attributes: character device, open/close, level 1, generic IOCTL
        dw      offset strategy_entry   ; strategy entry point
        dw      0                       ; no inter-device communication entry
        db      'NDISWDG$'              ; device name (= DRIVERNAME in PROTOCOL.INI)
        db      8 dup (0)               ; reserved
        dd      0                       ; capabilities bit strip (none)

; selector of the data group, resolved by the loader: the module's "DS" in the
; characteristics table and the segment of the table pointers handed to the
; Protocol Manager.
_dgroup_sel dw  DGROUP

; Far pointers to our entry points. Written with assembler references so that the
; loader fixes them up to the Ring 0 code selector. A far pointer built by C code at
; INIT time would contain the Ring 3 selector of the code that is running at that moment.
        public  _fptab
_fptab  dd      SYSREQ
        dd      WDGREQUEST
        dd      WDGTRANSMITCHAIN
        dd      WDGTRANSFERDATA
        dd      WDGRECEIVERELEASE
        dd      WDGINDICATIONON
        dd      WDGINDICATIONOFF
        dd      WDGREQUESTCONFIRM
        dd      WDGTRANSMITCONFIRM
        dd      WDGRECEIVELOOKAHEAD
        dd      WDGINDICATIONCOMPLETE
        dd      WDGRECEIVECHAIN
        dd      WDGSTATUS
_DATA   ends

CONST   segment word public 'DATA'
CONST   ends
CONST2  segment word public 'DATA'
CONST2  ends
_BSS    segment word public 'BSS'
_BSS    ends

DGROUP  group   _DATA, CONST, CONST2, _BSS

_TEXT   segment word public 'CODE'
        assume  cs:_TEXT, ds:DGROUP
        extrn   _strategy:near

; Strategy entry: ES:BX points at the request packet. The C routine takes the
; far pointer on the stack (cdecl); the kernel already provides DS, but it is
; reloaded here so that the C code never depends on it.
        public  strategy_entry
strategy_entry proc far
        push    ds
        push    es
        push    bx
        mov     ax, DGROUP
        mov     ds, ax
        call    _strategy
        add     sp, 4
        pop     ds
        ret
strategy_entry endp
_TEXT   ends

        end
