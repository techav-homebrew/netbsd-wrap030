/*	$NetBSD: wdc_mb.c,v 1.41 2019/06/29 16:41:19 tsutsui Exp $	*/

/*-
 * Copyright (c) 1998, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Onno van der Linden.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: wdc_mb.c,v 1.41 2019/06/29 16:41:19 tsutsui Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <sys/bswap.h>
#include <machine/cpu.h>
#include <sys/bus.h>
/* #include <machine/iomap.h>
#include <machine/mfp.h>
#include <machine/dma.h> */

#include <machine/bus.h>

#include <m68k/asm_single.h>

#include <dev/ata/atavar.h>
#include <dev/ic/wdcvar.h>

/* Wrap030 IDE register locations (base and offsets). */
#define WRAP030_WD_BASE			0x80200000
#define WRAP030_WD_CMD			(WRAP030_WD_BASE + 0)
#define WRAP030_WD_CTL_OFFSET	0x00002000
#define WRAP030_WD_CTL			(WRAP030_WD_BASE + WRAP030_WD_CTL_OFFSET)
#define WRAP030_WD_LEN			0x10
#define WRAP030_WD_REG_STRIDE 	2
#define MAXDISK 				2

/*
 * XXX This code currently doesn't even try to allow 32-bit data port use.
 */

struct wdc_mb_softc {
	struct wdc_softc sc_wdcdev;
	struct ata_channel *sc_chanlist[1];
	struct ata_channel sc_channel;
	struct wdc_regs sc_wdc_regs;
	void *sc_ih;
};

int	wdc_mb_match(device_t, cfdata_t, void *);
static void	wdc_mb_attach(device_t, device_t, void *);

struct wdc_regs gbl_wdr[MAXDISK];

CFATTACH_DECL_NEW(wdc_mb, sizeof(struct wdc_mb_softc),
    wdc_mb_match, wdc_mb_attach, NULL, NULL);

int
wdc_mb_match(device_t parent, cfdata_t match, void *aux)
{
	struct wdc_regs wdr;
	int result = 0, i;

	wdr.cmd_iot = wdr.ctl_iot = WRAP030_BUS_SPACE_EIO;

	/* get bus space handle for I/O address
	 * (on wrap030, this just copies the second parameter
	 * to the last parameter and returns OK. Should never
	 * return error here.) 
	 */
	if(bus_space_map(wdr.cmd_iot, WRAP030_WD_BASE, 
		WRAP030_WD_CTL_OFFSET + WRAP030_WD_LEN, 0, 
		&wdr.cmd_baseioh)) goto out;
	
	/* get pointers to each of the disk registers 
	 * (on wrap030, bus_space_subregion adds the input
	 * handle to the offset and returns that. Should
	 * never return error here.)
	 */
	for(i = 0; i < WDC_NREG; i++)
	{
		if(bus_space_subregion(wdr.cmd_iot, wdr.cmd_baseioh,
			i * WRAP030_WD_REG_STRIDE, WRAP030_WD_REG_STRIDE,
			&wdr.cmd_iohs[i]) != 0)
		{
			goto outunmap;
		}
	}

	/* let the driver initialize its data */
	wdc_init_shadow_regs(&wdr);

	/* get pointer to control registers
	 * (again, this will just return base+offset
	 * and should never return error.)
	 */
	if(bus_space_subregion(wdr.cmd_iot, wdr.cmd_baseioh,
		WRAP030_WD_CTL_OFFSET,WRAP030_WD_LEN,&wdr.ctl_ioh)) goto outunmap;
	
	/* let the driver check for disks */
	result = wdcprobe(&wdr);

outunmap:
	/* unmap does nothing here ... */
	bus_space_unmap(wdr.cmd_iot, wdr.cmd_baseioh,
		WRAP030_WD_CTL_OFFSET + WRAP030_WD_LEN);
out:
	return result;
}


static void
wdc_mb_attach(device_t parent, device_t self, void *aux)
{
	struct wdc_mb_softc *sc = device_private(self);
	struct wdc_regs *wdr;
	int i;

	sc->sc_wdcdev.sc_atac.atac_dev = self;
	sc->sc_wdcdev.regs = wdr = &sc->sc_wdc_regs;

	wdr->cmd_iot = wdr->ctl_iot = WRAP030_BUS_SPACE_EIO;

	/* map the entire io space */
	if(bus_space_map(wdr->cmd_iot, WRAP030_WD_BASE,
		(WRAP030_WD_CTL_OFFSET + WRAP030_WD_LEN), 0,
		&wdr->cmd_baseioh))
	{
		/* this should never happen ... */
		printf("wdc_mb_attach() couldn't map registers\r\n");
		return;
	}

	/* map the cmd registers subregion 
	 * & save pointers for each register
	 */
	for (i = 0; i < WDC_NREG; i++)
	{
		if (bus_space_subregion(wdr->cmd_iot, wdr->cmd_baseioh,
			i * WRAP030_WD_REG_STRIDE, WRAP030_WD_REG_STRIDE,
			&wdr->cmd_iohs[i]) != 0)
		{
			/* this should also never happen */
			printf("wdc_mb_attach() couldn't map subregion cmd reg\r\n");
			bus_space_unmap(wdr->cmd_iot, wdr->cmd_baseioh,
				WRAP030_WD_CTL_OFFSET + WRAP030_WD_LEN);
			return;
		}
	}

	/* map the aux registers subregion */
	if(bus_space_subregion(wdr->cmd_iot, wdr->cmd_baseioh,
		WRAP030_WD_CTL_OFFSET,WRAP030_WD_LEN,&wdr->ctl_ioh))
	{
		/* and this should never happen either */
		printf("wdc_mb_attach() couldn't map subregion aux reg\r\n");
		bus_space_unmap(wdr->cmd_iot, wdr->cmd_baseioh,
			WRAP030_WD_CTL_OFFSET + WRAP030_WD_LEN);
		return;
	}

	/* set up all the things */
	sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DATA16 |
		ATAC_CAP_ATA_NOSTREAM | ATAC_CAP_NOIRQ;
	sc->sc_wdcdev.sc_atac.atac_pio_cap = 0;
	sc->sc_chanlist[0] = &sc->sc_channel;
	sc->sc_wdcdev.sc_atac.atac_channels = sc->sc_chanlist;
	sc->sc_wdcdev.sc_atac.atac_nchannels = 1;
	sc->sc_wdcdev.wdc_maxdrives = 2;
	sc->sc_channel.ch_channel = 0;
	sc->sc_channel.ch_atac = &sc->sc_wdcdev.sc_atac;

	/* let driver initialize registers */
	wdc_init_shadow_regs(wdr);

	/* attach the disk */
	wdcattach(&sc->sc_channel);
}

/*
 * XXX
 * This piece of uglyness is caused by the fact that the byte lanes of
 * the data-register are swapped on the atari. This works OK for an IDE
 * disk, but turns into a nightmare when used on atapi devices.
 */
/* #define calc_addr(base, off, stride, wm)	\
	((u_long)(base) + ((off) << (stride)) + (wm))

static void
read_multi_2_swap(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint16_t *a, bus_size_t c)
{
	volatile uint16_t *ba;

	ba = (volatile uint16_t *)calc_addr(h, o, t->stride, t->wo_2);
	for (; c; a++, c--)
		*a = bswap16(*ba);
}

static void
write_multi_2_swap(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    const uint16_t *a, bus_size_t c)
{
	volatile uint16_t *ba;

	ba = (volatile uint16_t *)calc_addr(h, o, t->stride, t->wo_2);
	for (; c; a++, c--)
		*ba = bswap16(*a);
} */
