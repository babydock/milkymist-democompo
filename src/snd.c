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
#include <hw/ac97.h>
#include <hw/interrupts.h>

#include "snd.h"

/****************************************************************/
/* GENERAL                                                      */
/****************************************************************/

/*
 * We use interrupts and busy wait on those variables
 * (which end up in the data cache) rather than
 * busy waiting on the CSR, which would use bus bandwidth
 * for nothing.
 */
static volatile int snd_cr_request;
static volatile int snd_cr_reply;

void snd_init()
{
	unsigned int codec_id;
	unsigned int mask;
	
	snd_cr_request = 0;
	snd_cr_reply = 0;
	CSR_AC97_DCTL = 0;
	CSR_AC97_UCTL = 0;
	
	mask = irq_getmask();
	mask |= IRQ_AC97CRREQUEST|IRQ_AC97CRREPLY|IRQ_AC97DMAR|IRQ_AC97DMAW;
	irq_setmask(mask);
	
	codec_id = snd_ac97_read(0x00);
	if(codec_id == 0x0d50)
		printf("SND: found LM4550 AC'97 codec\n");
	else
		printf("SND: warning, unknown codec found (ID:%04x)\n", codec_id);
	
	/* Unmute and set volumes */
	/* TODO: API for this */
	snd_ac97_write(0x02, 0x0000);
	snd_ac97_write(0x04, 0x0f0f);
	snd_ac97_write(0x18, 0x0000);
	snd_ac97_write(0x0e, 0x0000);
	snd_ac97_write(0x1c, 0x0f0f);

	snd_play_empty();
	snd_record_empty();
	
	printf("SND: initialization complete\n");
}

void snd_isr_crrequest()
{
	snd_cr_request = 1;
	CSR_AC97_CRCTL = AC97_CRCTL_REQUEST; /* Ack interrupt */
}

void snd_isr_crreply()
{
	snd_cr_reply = 1;
	CSR_AC97_CRCTL = AC97_CRCTL_REPLY; /* Ack interrupt */
}

unsigned int snd_ac97_read(unsigned int addr)
{
	CSR_AC97_CRADDR = addr;
	snd_cr_reply = 0;
	CSR_AC97_CRCTL = AC97_CRCTL_RQEN;
	while(!snd_cr_reply);
	return CSR_AC97_CRDATAIN;
}

void snd_ac97_write(unsigned int addr, unsigned int value)
{
	CSR_AC97_CRADDR = addr;
	CSR_AC97_CRDATAOUT = value;
	snd_cr_request = 0;
	CSR_AC97_CRCTL = AC97_CRCTL_RQEN|AC97_CRCTL_WRITE;
	while(!snd_cr_request);
}

/****************************************************************/
/* PLAYING                                                      */
/****************************************************************/

static snd_callback play_callback;
static void *play_user;
static unsigned int play_nbytes;

#define PLAY_BUFQ_SIZE 4 /* < must be a power of 2 */
#define PLAY_BUFQ_MASK (PLAY_BUFQ_SIZE-1)

static short *play_queue[PLAY_BUFQ_SIZE];
static unsigned int play_produce;
static unsigned int play_consume;
static unsigned int play_level;
static int play_underrun;

static void play_start(short *buffer)
{
	CSR_AC97_DADDRESS = (unsigned int)buffer;
	CSR_AC97_DREMAINING = play_nbytes;
	CSR_AC97_DCTL = AC97_SCTL_EN; /* Start/ack interrupt */
}

void snd_isr_dmar()
{
	/* NB. the callback can give us buffers by calling snd_play_refill() */
	play_callback(play_queue[play_consume], play_user);

	play_consume = (play_consume + 1) & PLAY_BUFQ_MASK;
	play_level--;

	if(play_level > 0)
		play_start(play_queue[play_consume]);
	else {
		printf("SND: playing underrun\n");
		play_underrun = 1;
		CSR_AC97_DCTL = AC97_SCTL_EN; /* Ack interrupt anyway */
	}
}

void snd_play_empty()
{
	play_produce = 0;
	play_consume = 0;
	play_level = 0;
	play_underrun = 0;
}

int snd_play_refill(short *buffer)
{
	unsigned int oldmask;

	oldmask = irq_getmask();
	irq_setmask(oldmask & (~IRQ_AC97DMAR));
	
	if(play_level >= PLAY_BUFQ_SIZE) {
		irq_setmask(oldmask);
		printf("SND: play bufq overflow\n");
		return 0;
	}

	play_queue[play_produce] = buffer;
	play_produce = (play_produce + 1) & PLAY_BUFQ_MASK;
	play_level++;

	if(play_underrun) {
		play_underrun = 0;
		play_start(buffer);
	}

	irq_setmask(oldmask);
	
	return 1;
}

void snd_play_start(snd_callback callback, unsigned int nsamples, void *user)
{
	if(snd_play_active()) {
		printf("SND: snd_play_start called while already playing\n");
		return;
	}
	
	play_callback = callback;
	play_user = user;
	play_nbytes = nsamples*4;

	if(play_level > 0)
		play_start(play_queue[play_consume]);
	else {
		printf("SND: playing underrun\n");
		play_underrun = 1;
	}
}

void snd_play_stop()
{
	unsigned int oldmask;

	oldmask = irq_getmask();
	irq_setmask(oldmask & (~IRQ_AC97DMAR));
	CSR_AC97_DCTL = 0; /* this also acks any pending IRQ in the AC97 core */
	irq_ack(IRQ_AC97DMAR);
	irq_setmask(oldmask);
}

int snd_play_active()
{
	return(CSR_AC97_DCTL & AC97_SCTL_EN);
}

/****************************************************************/
/* RECORDING                                                    */
/****************************************************************/

static snd_callback record_callback;
static void *record_user;

static unsigned int record_nbytes;

#define RECORD_BUFQ_SIZE 4 /* < must be a power of 2 */
#define RECORD_BUFQ_MASK (RECORD_BUFQ_SIZE-1)

static short *record_queue[RECORD_BUFQ_SIZE];
static unsigned int record_produce;
static unsigned int record_consume;
static unsigned int record_level;
static int record_overrun;

static void record_start(short *buffer)
{
	CSR_AC97_UADDRESS = (unsigned int)buffer;
	CSR_AC97_UREMAINING = record_nbytes;
	CSR_AC97_UCTL = AC97_SCTL_EN; /* Start/ack interrupt */
}

void snd_isr_dmaw()
{
	asm volatile( /* Invalidate Level-1 data cache */
		"wcsr DCC, r0\n"
		"nop\n"
	);
	
	/* NB. the callback can give us buffers by calling snd_record_refill() */
	record_callback(record_queue[record_consume], record_user);

	record_consume = (record_consume + 1) & RECORD_BUFQ_MASK;
	record_level--;

	if(record_level > 0)
		record_start(record_queue[record_consume]);
	else {
		printf("SND: recording overrun\n");
		record_overrun = 1;
		CSR_AC97_UCTL = AC97_SCTL_EN; /* Ack interrupt anyway */
	}
}

void snd_record_empty()
{
	record_produce = 0;
	record_consume = 0;
	record_level = 0;
	record_overrun = 0;
}

int snd_record_refill(short *buffer)
{
	unsigned int oldmask;

	oldmask = irq_getmask();
	irq_setmask(oldmask & (~IRQ_AC97DMAW));

	if(record_level >= RECORD_BUFQ_SIZE) {
		irq_setmask(oldmask);
		printf("SND: record bufq overflow\n");
		return 0;
	}

	record_queue[record_produce] = buffer;
	record_produce = (record_produce + 1) & RECORD_BUFQ_MASK;
	record_level++;

	if(record_overrun) {
		record_overrun = 0;
		record_start(buffer);
	}

	irq_setmask(oldmask);

	return 1;
}

void snd_record_start(snd_callback callback, unsigned int nsamples, void *user)
{
	if(snd_record_active()) {
		printf("SND: snd_record_start called while already recording\n");
		return;
	}

	record_callback = callback;
	record_user = user;
	record_nbytes = nsamples*4;

	if(record_level > 0)
		record_start(record_queue[record_consume]);
	else {
		printf("SND: recording overrun\n");
		record_overrun = 1;
	}
}

void snd_record_stop()
{
	unsigned int oldmask;

	oldmask = irq_getmask();
	irq_setmask(oldmask & (~IRQ_AC97DMAW));
	CSR_AC97_UCTL = 0; /* this also acks any pending IRQ in the AC97 core */
	irq_ack(IRQ_AC97DMAW);
	irq_setmask(oldmask);
}

int snd_record_active()
{
	return(CSR_AC97_UCTL & AC97_SCTL_EN);
}
