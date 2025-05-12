/* $NetBSD: clock.c,v 1.6 2009/03/18 10:22:27 cegger Exp $ */

/*
 * Copyright (c) 1997, 1999
 *	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: clock.c,v 1.6 2009/03/18 10:22:27 cegger Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <machine/autoconf.h>
#include <machine/bus.h>

char *clockbase;

u_int hzcount;

extern void sic_enable_int(int, int, int, int, int);



/*
 * re-load the clock timer with hzcount to start it counting again
 */
void reloadclock(void);

void reloadclock(void)
{
	/* 
	 * ... I think this will work. It's not the proper way to access hardware 
	 * but it's not exactly proper timer hardware either
	 */
	*(clockbase + hzcount) = 0;
}

/*
 * Initialize interval timer. our timer is dumb. only one channel and it only 
 * counts down from whatever we set.
 */
void
cpu_initclocks(void)
{
	/* this really shouldn't be here, but I'll run with it -- techav */
	/* mainbus_map(0x5a000000, 0x1000, 0, (void *)&clockbase); */
	clockbase = (char *)0x80f00000;
	mainbus_map((bus_space_tag_t)WRAP030_BUS_SPACE_EIO, 0x80f00000, 0, (void *)&clockbase);

	/* what does this do?
	 * ... it's defined in arch/cesfic/cesfic/sic6351.c so it's something 
	 * to do with the interrupt controller
	 * "SIC 6351 is an ASIC acting as a complete system interrupt controller"
	 * so we don't need this for wrap030
	 */
	/* sic_enable_int(25, 0, 1, 6, 0); */

	/* we're running at 25MHz; a 1ms timer is 25000 count ... roughly */
	hzcount = 25000;

	/* is this the right way to do this? */
	reloadclock();
}

void
setstatclockrate(int newhz)
{
}

/* this seems rather silly... */
/*     i agree ... also, what is this even for? --techav   */
/*
void otherclock(int);

void
otherclock(int sr)
{
	printf("otherclock(%x)\n", sr);
}
*/
