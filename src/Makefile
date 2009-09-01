MMDIR?=../..
include $(MMDIR)/software/include.mak

OBJECTS=crt0.o isr.o brd.o time.o mem.o vga.o snd.o slowout.o hdlcd.o tmu.o pfpu.o line.o main.o
SEGMENTS=-j .text -j .data -j .rodata

all: boot.bin

%.bin: %.elf
	$(OBJCOPY) $(SEGMENTS) -O binary $< $@
	$(MMDIR)/tools/crc32 $@

boot.elf: linker.ld $(OBJECTS) $(MMDIR)/software/baselib/baselib.a $(MMDIR)/software/mathlib/mathlib.a
	$(LD) $(LDFLAGS) -T linker.ld -N -o $@ $(OBJECTS) $(MMDIR)/software/baselib/baselib.a $(MMDIR)/software/mathlib/mathlib.a

.PHONY: clean depend

depend:
	makedepend -Y -- $(CFLAGS) -- *.c

clean:
	rm -f $(OBJECTS) boot.elf boot.bin .*~ *~ Makefile.bak
