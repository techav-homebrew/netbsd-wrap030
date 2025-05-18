/*
 * Copyright 2006 Kyma Systems LLC.
 * All rights reserved.
 *
 * Written by Sanjay Lal <sanjayl@kymasys.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Kyma Systems LLC.
 * 4. The name of Kyma Systems LLC may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KYMA SYSTEMS LLC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL KYMA SYSTEMS LLC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* octocom 16550 com devices attachment to mainbus for wrap030 
 * ported from sys/arch/macppc/dev/com_mainbus.c */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/tty.h>

#include <sys/bus.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

#include <machine/autoconf.h>

/* there's probably a better place to define these ... */
#define MAXCOM 8    
#define COMFREQ 1843200

struct com_mainbus_softc
{
    struct com_softc sc_com;    /* real "com" softc */
    void *sc_ih;                /* interrupt handler */
};

int com_mainbus_probe(device_t, cfdata_t , void *);
void com_mainbus_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(com_mainbus, sizeof(struct com_mainbus_softc),
              com_mainbus_probe, com_mainbus_attach, NULL, NULL);

int com_mainbus_attached = 0;

int
com_mainbus_probe(device_t parent, cfdata_t match, void *aux)
{
    /* printf("com_mainbus_probe(%p,%p,%p)\r\n",
        parent,match,aux); */
    if(com_mainbus_attached < MAXCOM)
    {
        com_mainbus_attached++;
        /* printf("com_mainbus_probe() good. done.\r\n"); */
        return(1);
    }
    else
    {
        /* printf("com_mainbus_probe() bad. done.\r\n"); */
        return(0);
    }
}

void
com_mainbus_attach(device_t parent, device_t self, void *aux)
{
    struct com_mainbus_softc *msc = device_private(self);
    struct com_softc *sc = &msc->sc_com;

    /* printf("com_mainbus_attach(%p,%p,%p)\r\n",parent,self,aux); */
    
    bus_space_tag_t iot;
    bus_space_handle_t ioh;
    bus_addr_t iobase;

    sc->sc_dev = self;

    /* fun fact for later, 'sc->sc_tty' is a pointer of type 'struct tty' */

    iot = (bus_space_tag_t)WRAP030_BUS_SPACE_EIO;
    
    /* each UART space is 8 consecutive addresses long */
    iobase = 0x80300000 + device_unit(self) * 0x08;

    /* I only want the first unit to be the console ... I think? */
    if(device_unit(self) == 0)
    {
        /* comcnattach is defined in sys/dev/ic/com.c */
        comcnattach(iot, iobase, 115200, COMFREQ, COM_TYPE_NORMAL, (CREAD | CS8));
    }

    /* map the bus space consumed by this device  */
    /* printf("com_mainbus_attach() bus_space_map(%p,%p,%u,0,%u)\r\n",
        iot,iobase,COM_NPORTS,&ioh); */
    bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh);

    /* com_init_regs is defined in com.c */
    /* printf("com_mainbus_attach() com_init_regs(%p,%p,%p,%p)\r\n",
        &sc->sc_regs, iot, ioh, iobase); */
    com_init_regs(&sc->sc_regs, iot, ioh, iobase);

    sc->sc_frequency = COMFREQ;

    /* another function from com.c; this one will initialize the device */
    /* printf("com_mainbus_attach() com_attach_subr(%p)\r\n",sc); */
    com_attach_subr(sc);

    /* this is where we will establish interrupt handlers when we have them */
    /* msc->sc_ih = .... ?
     * if (msc->sc_ih == NULL) panic("failed to establish int handler");
     */
    /* printf("com_mainbus_attach() done.\r\n"); */
}
