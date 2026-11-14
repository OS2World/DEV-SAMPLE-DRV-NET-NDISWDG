# Makefile.wat - Open Watcom 2.0 build file for NDISWDG.OS2 (NDIS 2.0.1 pass-through module)
# Run from the project root:  wmake -f Makefile.wat   (or use compile_wat.cmd)
#
# Tools: wcc (16-bit C), wasm (assembler), wlink. No C library is linked.
# Output: bin\ndiswdg.os2 (16-bit NE device driver) and bin\ndiswdg.map

SRC  = src
OBJ  = bin
NAME = ndiswdg

# 16-bit C: small model, 286, no stack checks, SS != DS (device driver stack),
# packed structures, no default library, errors only
CC      = wcc
CFLAGS  = -ms -2 -s -zu -zl -zp1 -zq -ox -d0 -bt=os2

AS      = wasm
ASFLAGS = -zq -2p

# hdr must be linked first and tail last (device header at offset 0, end markers)
OBJS = $(OBJ)\hdr.obj $(OBJ)\init.obj $(OBJ)\ndis.obj $(OBJ)\tail.obj

HDRS = $(SRC)\ndis.h $(SRC)\version.h

all: $(OBJ)\$(NAME).os2 $(OBJ)\wdgstat.exe .SYMBOLIC

$(OBJ)\hdr.obj: $(SRC)\hdr.asm
	-@mkdir $(OBJ) > nul 2>&1
	$(AS) $(ASFLAGS) -fo=$@ $(SRC)\hdr.asm

$(OBJ)\tail.obj: $(SRC)\tail.asm
	-@mkdir $(OBJ) > nul 2>&1
	$(AS) $(ASFLAGS) -fo=$@ $(SRC)\tail.asm

$(OBJ)\init.obj: $(SRC)\init.c $(HDRS)
	-@mkdir $(OBJ) > nul 2>&1
	$(CC) $(CFLAGS) -fo=$@ $(SRC)\init.c

$(OBJ)\ndis.obj: $(SRC)\ndis.c $(HDRS)
	-@mkdir $(OBJ) > nul 2>&1
	$(CC) $(CFLAGS) -fo=$@ $(SRC)\ndis.c

$(OBJ)\$(NAME).os2: $(OBJS) $(SRC)\bldlevel.lnk
	@%create $(OBJ)\$(NAME).lnk
	@%append $(OBJ)\$(NAME).lnk system os2 dll
	@%append $(OBJ)\$(NAME).lnk option protmode
	@%append $(OBJ)\$(NAME).lnk name $(OBJ)\$(NAME).os2
	@%append $(OBJ)\$(NAME).lnk @$(SRC)\bldlevel.lnk
	@%append $(OBJ)\$(NAME).lnk option quiet
	@%append $(OBJ)\$(NAME).lnk option caseexact
	@%append $(OBJ)\$(NAME).lnk option nodefaultlibs
	@%append $(OBJ)\$(NAME).lnk option map=$(OBJ)\$(NAME).map
	@%append $(OBJ)\$(NAME).lnk segment class 'CODE' iopl
	@%append $(OBJ)\$(NAME).lnk file $(OBJ)\hdr.obj
	@%append $(OBJ)\$(NAME).lnk file $(OBJ)\init.obj
	@%append $(OBJ)\$(NAME).lnk file $(OBJ)\ndis.obj
	@%append $(OBJ)\$(NAME).lnk file $(OBJ)\tail.obj
	@%append $(OBJ)\$(NAME).lnk library C:\WATCOM\lib286\os2\os2.lib
	wlink @$(OBJ)\$(NAME).lnk

# status tool (32-bit OS/2 program, see sample\wdgstat.c)
$(OBJ)\wdgstat.exe: sample\wdgstat.c
	-@mkdir $(OBJ) > nul 2>&1
	wcc386 -zq -bt=os2 -i=$(%WATCOM)\h -i=$(%WATCOM)\h\os2 -fo=$(OBJ)\wdgstat.obj sample\wdgstat.c
	wlink system os2v2 name $(OBJ)\wdgstat.exe file $(OBJ)\wdgstat.obj option quiet

clean: .SYMBOLIC
	-@echo y | del $(OBJ)\*.* > nul 2>&1
