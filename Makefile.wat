# Makefile.wat - Open Watcom 2.0 build file for the DWB Generic Model NDIS Wedge driver
# Run from the project root:  wmake -f Makefile.wat   (or use compile_wat.cmd)
#
# Tools: wasm (Open Watcom assembler, MASM compatible), wlink.
# Output: bin\wedge.os2 (16-bit NE module) and bin\wedge.map

SRC  = src
OBJ  = bin
NAME = wedge

# Open Watcom assembler: 80286 protected mode, OS/2 build, include path src
AS      = wasm
ASFLAGS = -zq -2p -dBUILDOS2 -i=$(SRC)

OBJS = $(OBJ)\sstrat.obj $(OBJ)\smsgs.obj $(OBJ)\sinit.obj $(OBJ)\sdata.obj &
       $(OBJ)\sndiinit.obj $(OBJ)\sdevhlp.obj $(OBJ)\sysreq.obj $(OBJ)\protcall.obj &
       $(OBJ)\maccalls.obj $(OBJ)\devioctl.obj

INCS = $(SRC)\build.cfg $(SRC)\segments.inc $(SRC)\sstrucs.inc $(SRC)\smacros.inc &
       $(SRC)\os2equ.inc $(SRC)\devhlp.inc $(SRC)\devhdr.inc $(SRC)\ioctl.inc $(SRC)\wedge.inc

.asm: $(SRC)

all: $(OBJ) $(OBJ)\$(NAME).os2 .SYMBOLIC

$(OBJ): .SYMBOLIC
	-@mkdir $(OBJ) > nul 2>&1

$(OBJ)\sstrat.obj:    $(SRC)\sstrat.asm    $(INCS)
	$(AS) $(ASFLAGS) -fo=$@ $(SRC)\sstrat.asm
$(OBJ)\smsgs.obj:     $(SRC)\smsgs.asm     $(INCS)
	$(AS) $(ASFLAGS) -fo=$@ $(SRC)\smsgs.asm
$(OBJ)\sinit.obj:     $(SRC)\sinit.asm     $(INCS)
	$(AS) $(ASFLAGS) -fo=$@ $(SRC)\sinit.asm
$(OBJ)\sdata.obj:     $(SRC)\sdata.asm     $(INCS)
	$(AS) $(ASFLAGS) -fo=$@ $(SRC)\sdata.asm
$(OBJ)\sndiinit.obj:  $(SRC)\sndiinit.asm  $(INCS)
	$(AS) $(ASFLAGS) -fo=$@ $(SRC)\sndiinit.asm
$(OBJ)\sdevhlp.obj:   $(SRC)\sdevhlp.asm   $(INCS)
	$(AS) $(ASFLAGS) -fo=$@ $(SRC)\sdevhlp.asm
$(OBJ)\sysreq.obj:    $(SRC)\sysreq.asm    $(INCS)
	$(AS) $(ASFLAGS) -fo=$@ $(SRC)\sysreq.asm
$(OBJ)\protcall.obj:  $(SRC)\protcall.asm  $(INCS)
	$(AS) $(ASFLAGS) -fo=$@ $(SRC)\protcall.asm
$(OBJ)\maccalls.obj:  $(SRC)\maccalls.asm  $(INCS)
	$(AS) $(ASFLAGS) -fo=$@ $(SRC)\maccalls.asm
$(OBJ)\devioctl.obj:  $(SRC)\devioctl.asm  $(INCS)
	$(AS) $(ASFLAGS) -fo=$@ $(SRC)\devioctl.asm

# Link: same object order as the original WEDGEOS2.LNK
$(OBJ)\$(NAME).os2: $(OBJS) $(SRC)\bldlevel.lnk
	@%create $(OBJ)\$(NAME).lnk
	@%append $(OBJ)\$(NAME).lnk system os2 dll
	@%append $(OBJ)\$(NAME).lnk name $(OBJ)\$(NAME).os2
	@%append $(OBJ)\$(NAME).lnk option protmode
	@%append $(OBJ)\$(NAME).lnk @$(SRC)\bldlevel.lnk
	@%append $(OBJ)\$(NAME).lnk option quiet
	@%append $(OBJ)\$(NAME).lnk option caseexact
	@%append $(OBJ)\$(NAME).lnk option nodefaultlibs
	@%append $(OBJ)\$(NAME).lnk option map=$(OBJ)\$(NAME).map
	@%append $(OBJ)\$(NAME).lnk file $(OBJ)\sstrat.obj
	@%append $(OBJ)\$(NAME).lnk file $(OBJ)\smsgs.obj
	@%append $(OBJ)\$(NAME).lnk file $(OBJ)\sdata.obj
	@%append $(OBJ)\$(NAME).lnk file $(OBJ)\sndiinit.obj
	@%append $(OBJ)\$(NAME).lnk file $(OBJ)\sdevhlp.obj
	@%append $(OBJ)\$(NAME).lnk file $(OBJ)\sysreq.obj
	@%append $(OBJ)\$(NAME).lnk file $(OBJ)\protcall.obj
	@%append $(OBJ)\$(NAME).lnk file $(OBJ)\sinit.obj
	@%append $(OBJ)\$(NAME).lnk file $(OBJ)\maccalls.obj
	@%append $(OBJ)\$(NAME).lnk file $(OBJ)\devioctl.obj
	@%append $(OBJ)\$(NAME).lnk library C:\os2\doscalls.lib
	wlink @$(OBJ)\$(NAME).lnk

clean: .SYMBOLIC
	-@echo y | del $(OBJ)\*.* > nul 2>&1
