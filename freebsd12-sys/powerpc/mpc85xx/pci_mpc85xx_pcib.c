/*-
 * Copyright 2015 Justin Hibbits
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * From: FreeBSD: src/sys/powerpc/mpc85xx/pci_ocp.c,v 1.9 2010/03/23 23:46:28 marcel
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/endian.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>

#include <machine/intr_machdep.h>

#include "pcib_if.h"

struct fsl_pcib_softc {
        /*
         * This is here so that we can use pci bridge methods, too - the
         * generic routines only need the dev, secbus and subbus members
         * filled.
         */
        struct pcib_softc       ops_pcib_sc;
	phandle_t		ops_node;
        struct ofw_bus_iinfo    ops_iinfo;
};

static int
fsl_pcib_rc_probe(device_t dev)
{

	if (pci_get_vendor(dev) != 0x1957)
		return (ENXIO);
	if (pci_get_progif(dev) != 0)
		return (ENXIO);
	if (pci_get_class(dev) != PCIC_PROCESSOR)
		return (ENXIO);
	if (pci_get_subclass(dev) != PCIS_PROCESSOR_POWERPC)
		return (ENXIO);

	return (BUS_PROBE_DEFAULT);
}

static int
fsl_pcib_rc_attach(device_t dev)
{
	struct fsl_pcib_softc *sc;

	sc = device_get_softc(dev);
	sc->ops_pcib_sc.dev = dev;
	sc->ops_node = ofw_bus_get_node(dev);

	ofw_bus_setup_iinfo(sc->ops_node, &sc->ops_iinfo,
	    sizeof(cell_t));

	pcib_attach_common(dev);
	return (pcib_attach_child(dev));
}

static phandle_t
fsl_pcib_rc_get_node(device_t bridge, device_t dev)
{
	/* We have only one child, the PCI bus, so pass it our node */

	return (ofw_bus_get_node(bridge));
}

static int
fsl_pcib_rc_route_interrupt(device_t bridge, device_t dev, int intpin)
{
	struct fsl_pcib_softc *sc;
	struct ofw_bus_iinfo *ii;
	struct ofw_pci_register reg;
	cell_t pintr, mintr[2];
	int intrcells;
	phandle_t iparent;

	sc = device_get_softc(bridge);
	ii = &sc->ops_iinfo;
	if (ii->opi_imapsz > 0) {
		pintr = intpin;

		/* Fabricate imap information if this isn't an OFW device */
		bzero(&reg, sizeof(reg));
		reg.phys_hi = (pci_get_bus(dev) << OFW_PCI_PHYS_HI_BUSSHIFT) |
		    (pci_get_slot(dev) << OFW_PCI_PHYS_HI_DEVICESHIFT) |
		    (pci_get_function(dev) << OFW_PCI_PHYS_HI_FUNCTIONSHIFT);

		intrcells = ofw_bus_lookup_imap(ofw_bus_get_node(dev), ii, &reg,
		    sizeof(reg), &pintr, sizeof(pintr), mintr, sizeof(mintr),
		    &iparent);
		if (intrcells) {
			/*
			 * If we've found a mapping, return it and don't map
			 * it again on higher levels - that causes problems
			 * in some cases, and never seems to be required.
			 */
			mintr[0] = ofw_bus_map_intr(dev, iparent, intrcells,
			    mintr);
			return (mintr[0]);
		}
	} else if (intpin >= 1 && intpin <= 4) {
		/*
		 * When an interrupt map is missing, we need to do the
		 * standard PCI swizzle and continue mapping at the parent.
		 */
		return (pcib_route_interrupt(bridge, dev, intpin));
	}
	return (PCIB_ROUTE_INTERRUPT(device_get_parent(device_get_parent(
	    bridge)), bridge, intpin));
}

static device_method_t fsl_pcib_rc_methods[] = {
	DEVMETHOD(device_probe,		fsl_pcib_rc_probe),
	DEVMETHOD(device_attach,	fsl_pcib_rc_attach),

	/* pcib interface */
	DEVMETHOD(pcib_route_interrupt,	fsl_pcib_rc_route_interrupt),
	DEVMETHOD(pcib_request_feature,	pcib_request_feature_allow),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	fsl_pcib_rc_get_node),
	DEVMETHOD_END
};

static devclass_t fsl_pcib_rc_devclass;
DEFINE_CLASS_1(pcib, fsl_pcib_rc_driver, fsl_pcib_rc_methods,
    sizeof(struct fsl_pcib_softc), pcib_driver);
DRIVER_MODULE(rcpcib, pci, fsl_pcib_rc_driver, fsl_pcib_rc_devclass, 0, 0);