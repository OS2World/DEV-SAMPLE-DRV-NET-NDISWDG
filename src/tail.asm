; tail.asm - end-of-resident markers of NDISWDG.OS2
;
; Copyright (c) 2026, Martin Iturbide
; All rights reserved. BSD 3-Clause License, see LICENSE.
;
; This object must be linked LAST. The INIT command reports these offsets to the
; kernel as the end of the code and data that stay resident.

        .286p

_DATA   segment word public 'DATA'
_DATA   ends
CONST   segment word public 'DATA'
CONST   ends
CONST2  segment word public 'DATA'
CONST2  ends

_BSS    segment word public 'BSS'
        public  _end_of_data
_end_of_data label byte
_BSS    ends

DGROUP  group   _DATA, CONST, CONST2, _BSS

_TEXT   segment word public 'CODE'
        public  _end_of_code
_end_of_code label byte
_TEXT   ends

        end
