OUTPUT_FORMAT("elf32-lm32")
ENTRY(_start)

__DYNAMIC = 0;

MEMORY {
	flash : ORIGIN = 0x00000000, LENGTH = 0x800000   /* 8M */
	sram  : ORIGIN = 0x20000000, LENGTH = 0x1000     /* 4k */
	sdram : ORIGIN = 0x40000000, LENGTH = 0x4000000  /* 64M */
}

SECTIONS
{
	.text :
	{
		_ftext = .;
		*(.text .stub .text.* .gnu.linkonce.t.*)
		_etext = .;
	} > sdram

	.rodata :
	{
		. = ALIGN(4);
		_frodata = .;
		*(.rodata .rodata.* .gnu.linkonce.r.*)
		*(.rodata1)
		_erodata = .;
	} > sdram

	.data :
	{
		. = ALIGN(4);
		_fdata = .;
		*(.data .data.* .gnu.linkonce.d.*)
		*(.data1)
		_gp = ALIGN(16);
		*(.sdata .sdata.* .gnu.linkonce.s.*)
		_edata = .;
	} > sdram

	.bss :
	{
		. = ALIGN(4);
		_fbss = .;
		*(.dynsbss)
		*(.sbss .sbss.* .gnu.linkonce.sb.*)
		*(.scommon)
		*(.dynbss)
		*(.bss .bss.* .gnu.linkonce.b.*)
		*(COMMON)
		_ebss = .;
		_end = .;
	} > sdram
}

PROVIDE(_fstack = ORIGIN(sdram) + LENGTH(sdram) - 4);

