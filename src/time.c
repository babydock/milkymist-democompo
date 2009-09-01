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

/* SYSTEM CLOCK - Using HW timer 0 */

#include <libc.h>
#include <console.h>
#include <irq.h>
#include <board.h>
#include <hw/sysctl.h>
#include <hw/interrupts.h>

#include "brd.h"
#include "time.h"

static int sec;

void time_init()
{
	unsigned int mask;
	
	CSR_TIMER0_COUNTER = 0;
	CSR_TIMER0_COMPARE = brd_desc->clk_frequency;
	CSR_TIMER0_CONTROL = TIMER_AUTORESTART|TIMER_ENABLE;

	mask = irq_getmask();
	mask |= IRQ_TIMER0;
	irq_setmask(mask);

	printf("TIM: system timer started\n");
}

void time_isr()
{
	sec++;
	CSR_TIMER0_CONTROL = TIMER_AUTORESTART|TIMER_ENABLE; /* Ack interrupt */
}

void time_get(struct timestamp *ts)
{
	unsigned int oldmask = 0;
	unsigned int ctl, counter, sec2;

	oldmask = irq_getmask();
	irq_setmask(oldmask & ~(IRQ_TIMER0));
	counter = CSR_TIMER0_COUNTER;
	ctl = CSR_TIMER0_CONTROL;
	sec2 = sec;
	irq_setmask(oldmask);

	ts->sec = sec2;
	ts->usec = counter/(brd_desc->clk_frequency/1000000);
	
	/*
	 * If the counter is less than half a second, we consider that
	 * the overflow was already present when we read the counter
	 * value.
	 */
	if(ctl & TIMER_MATCH) {
		if(counter < (brd_desc->clk_frequency/2))
			ts->sec++;
	}
}

void time_add(struct timestamp *dest, struct timestamp *delta)
{
	dest->sec += delta->sec;
	dest->usec += delta->usec;
	if(dest->usec >= 1000000) {
		dest->sec++;
		dest->usec -= 1000000;
	}
}

void time_diff(struct timestamp *dest, struct timestamp *t1, struct timestamp *t0)
{
	dest->sec = t1->sec - t0->sec;
	dest->usec = t1->usec - t0->usec;
	if(dest->usec < 0) {
		dest->sec--;
		dest->usec += 1000000;
	}
}
