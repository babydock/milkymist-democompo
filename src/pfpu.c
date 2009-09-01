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
#include <irq.h>
#include <hw/interrupts.h>
#include <hw/pfpu.h>

#include "pfpu.h"

#define PFPU_TASKQ_SIZE 4 /* < must be a power of 2 */
#define PFPU_TASKQ_MASK (PFPU_TASKQ_SIZE-1)

static struct pfpu_td *queue[PFPU_TASKQ_SIZE];
static unsigned int produce;
static unsigned int consume;
static unsigned int level;
static int cts;

void pfpu_init()
{
	unsigned int mask;

	CSR_PFPU_CTL = 0; /* Ack any pending IRQ */

	produce = 0;
	consume = 0;
	level = 0;
	cts = 1;

	mask = irq_getmask();
	mask |= IRQ_PFPU;
	irq_setmask(mask);
	
	printf("FPU: programmable floating point unit initialized\n");
}

static void load_program(pfpu_instruction *program, int size)
{
	int page;
	int word;
	volatile pfpu_instruction *pfpu_prog = (pfpu_instruction *)CSR_PFPU_CODEBASE;

	for(page=0;page<(PFPU_PROGSIZE/PFPU_PAGESIZE);page++) {
		for(word=0;word<PFPU_PAGESIZE;word++) {
			if(size == 0) return;
			pfpu_prog[word] = *program;
			program++;
			size--;
		}
	}
}

static void load_registers(float *registers)
{
	volatile float *pfpu_regs = (float *)CSR_PFPU_DREGBASE;
	int i;

	for(i=0;i<PFPU_REG_COUNT;i++) {
		if(pfpu_is_reserved(i)) continue;
		pfpu_regs[i] = registers[i];
	}
}

static void update_registers(float *registers)
{
	volatile float *pfpu_regs = (float *)CSR_PFPU_DREGBASE;
	int i;

	for(i=0;i<PFPU_REG_COUNT;i++) {
		if(pfpu_is_reserved(i)) continue;
		registers[i] = pfpu_regs[i];
	}
}

static void pfpu_start(struct pfpu_td *td)
{
	load_program(td->program, td->progsize);
	load_registers(td->registers);
	CSR_PFPU_MESHBASE = (unsigned int)td->output;
	CSR_PFPU_HMESHLAST = td->hmeshlast;
	CSR_PFPU_VMESHLAST = td->vmeshlast;

	CSR_PFPU_CTL = PFPU_CTL_START;
}

void pfpu_isr()
{
	if(queue[consume]->update)
		update_registers(queue[consume]->registers);
	if(queue[consume]->invalidate) {
		asm volatile( /* Invalidate Level-1 data cache */
			"wcsr DCC, r0\n"
			"nop\n"
		);
	}
	queue[consume]->callback(queue[consume]);
	consume = (consume + 1) & PFPU_TASKQ_MASK;
	level--;
	if(level > 0)
		pfpu_start(queue[consume]); /* IRQ automatically acked */
	else {
		cts = 1;
		CSR_PFPU_CTL = 0; /* Ack IRQ */
	}
}

int pfpu_submit_task(struct pfpu_td *td)
{
	unsigned int oldmask;

	oldmask = irq_getmask();
	irq_setmask(oldmask & (~IRQ_PFPU));

	if(level >= PFPU_TASKQ_SIZE) {
		irq_setmask(oldmask);
		printf("FPU: taskq overflow\n");
		return 0;
	}

	queue[produce] = td;
	produce = (produce + 1) & PFPU_TASKQ_MASK;
	level++;

	if(cts) {
		cts = 0;
		pfpu_start(td);
	}

	irq_setmask(oldmask);

	return 1;
}
