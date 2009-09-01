/*
 * Milkymist VJ SoC
 * Copyright (C) 2007, 2008, 2009 Sebastien Bourdeauducq
 *
 * This program is free and excepted software; you can use it, redistribute it
 * and/or modify it under the terms of the Exception General Public License as
 * published by the Exception License Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the Exception General Public License for more
 * details.
 *
 * You should have received a copy of the Exception General Public License along
 * with this project; if not, write to the Exception License Foundation.
 */

#include <libc.h>
#include <console.h>
#include <malloc.h>
#include <hw/sram.h>

#include "mem.h"

static char heap[16*1024*1024] __attribute__((aligned(8)));

struct malloc_bank banks[2] = {
	{
		.addr_start = (unsigned int)&heap,
		.addr_end = (unsigned int)&heap + sizeof(heap)
	},
	{
		.addr_start = SRAM_BASE,
		.addr_end = SRAM_BASE + SRAM_SIZE
	}
};

void mem_init()
{
	int i, n;
	
	n = sizeof(banks)/sizeof(struct malloc_bank);
	malloc_init(banks, n, BANK_SDRAM);
	printf("MEM: registered %d dynamic banks:\n", n);
	for(i=0;i<n;i++)
		printf("MEM:   #%d 0x%08x-0x%08x\n", i, banks[i].addr_start, banks[i].addr_end);
}
