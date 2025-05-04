/*	$NetBSD: bus.c,v 1.67 2022/07/26 20:08:55 andvar Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center and by Chris G. Demetriou.
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

/* forked from arch/atari/atari/bus.c for wrap030 */

#include "opt_m68k_arch.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: bus.c,v 1.67 2022/07/26 20:08:55 andvar Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/extent.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/proc.h>

#include <uvm/uvm.h>

#include <machine/cpu.h>
#include <m68k/cacheops.h>

#include <machine/bus.h>

/*
 * Extent maps to manage all memory space, including I/O ranges.  Allocate
 * storage for 16 regions in each, initially.  Later, iomem_malloc_safe
 * will indicate that it's safe to use malloc() to dynamically allocate
 * region descriptors.
 * This means that the fixed static storage is only used for registrating
 * the found memory regions and the bus-mapping of the console.
 */
/* 
static long iomem_ex_storage[EXTENT_FIXED_STORAGE_SIZE(16) / sizeof(long)]; 
*/
static struct extent *iomem_ex;
static int iomem_malloc_safe = 0;

/* static int  _bus_dmamap_load_buffer(bus_dma_tag_t tag, bus_dmamap_t,
		void *, bus_size_t, struct vmspace *, int, paddr_t *,
		int *, int); */
static int  bus_mem_add_mapping(bus_space_tag_t t, bus_addr_t bpa,
		bus_size_t size, int flags, bus_space_handle_t *bsph);

extern paddr_t avail_end;


/*
 * We need these for the early memory allocator. The idea is this:
 * Allocate VA-space through ptextra (atari_init.c:startc()). When
 * The VA & size of this space are known, call bootm_init().
 * Until the VM-system is up, bus_mem_add_mapping() allocates its virtual
 * addresses from this extent-map.
 *
 * This allows for the console code to use the bus_space interface at a
 * very early stage of the system configuration.
 */
static pt_entry_t	*bootm_ptep;
static long		bootm_ex_storage[EXTENT_FIXED_STORAGE_SIZE(32) /
								sizeof(long)];
static struct extent	*bootm_ex;

static vaddr_t	bootm_alloc(paddr_t pa, u_long size, int flags);
static int	bootm_free(vaddr_t va, u_long size);

void bootm_init(vaddr_t, void*, vsize_t);

void
bootm_init(vaddr_t va, void *ptep, vsize_t size)
{

	bootm_ex = extent_create("bootmem", va, va + size,
	    (void *)bootm_ex_storage, sizeof(bootm_ex_storage),
	    EX_NOCOALESCE|EX_NOWAIT);
	bootm_ptep = (pt_entry_t *)ptep;
}

vaddr_t
bootm_alloc(paddr_t pa, u_long size, int flags)
{
	pt_entry_t	*pg, *epg;
	pt_entry_t	pg_proto;
	vaddr_t		va, rva;

	if (extent_alloc(bootm_ex, size, PAGE_SIZE, 0, EX_NOWAIT, &rva) != 0) {
		printf("bootm_alloc fails! Not enough fixed extents?\n");
		printf("Requested extent: pa=%lx, size=%lx\n",
						(u_long)pa, size);
		return 0;
	}
	
	pg  = &bootm_ptep[btoc(rva - bootm_ex->ex_start)];
	epg = &pg[btoc(size)];
	va  = rva;
	pg_proto = pa | PG_RW | PG_V;
	if ((flags & BUS_SPACE_MAP_CACHEABLE) == 0)
		pg_proto |= PG_CI;
	while (pg < epg) {
		*pg++     = pg_proto;
		pg_proto += PAGE_SIZE;
#if defined(M68040) || defined(M68060)
		if (mmutype == MMU_68040) {
			DCFP(pa);
			pa += PAGE_SIZE;
		}
#endif
		TBIS(va);
		va += PAGE_SIZE;
	}
	return rva;
}

int
bootm_free(vaddr_t va, u_long size)
{

	if ((va < bootm_ex->ex_start) || ((va + size) > bootm_ex->ex_end))
		return 0; /* Not for us! */
	extent_free(bootm_ex, va, size, EX_NOWAIT);
	return 1;
}



/* **** */

int
bus_space_map(bus_space_tag_t t, bus_addr_t bpa, bus_size_t size, int flags,
    bus_space_handle_t *mhp)
{
	int	error;

	/*
	 * Before we go any further, let's make sure that this
	 * region is available.
	 */
	/* error = extent_alloc_region(iomem_ex, bpa + t->base, size,
			EX_NOWAIT | iomem_malloc_safe); */
    error = extent_alloc_region(iomem_ex, bpa + t, size,
            EX_NOWAIT | iomem_malloc_safe);

	if (error != 0)
		return error;

	error = bus_mem_add_mapping(t, bpa, size, flags, mhp);
	if (error != 0) {
		/* if (extent_free(iomem_ex, bpa + t->base, size,
		    EX_NOWAIT | iomem_malloc_safe)) { */
        if (extent_free(iomem_ex, bpa + t, size,
            EX_NOWAIT | iomem_malloc_safe)) {
			printf("%s: pa 0x%lx, size 0x%lx\n",
			    __func__, bpa, size);
			printf("%s: can't free region\n", __func__);
		}
	}
	return error;
}



static int
bus_mem_add_mapping(bus_space_tag_t t, bus_addr_t bpa, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	vaddr_t	va;
	paddr_t	pa, endpa;

	/*
    pa    = m68k_trunc_page(bpa + t->base);
	endpa = m68k_round_page((bpa + t->base + size) - 1);
	*/

	pa    = m68k_trunc_page(bpa + t);
	endpa = m68k_round_page((bpa + t + size) - 1);

#ifdef DIAGNOSTIC
	if (endpa <= pa)
		panic("%s: overflow", __func__);
#endif

	if (kernel_map == NULL) {
		/*
		 * The VM-system is not yet operational, allocate from
		 * a special pool.
		 */
		va = bootm_alloc(pa, endpa - pa, flags);
		if (va == 0)
			return ENOMEM;
		*bshp = va + (bpa & PGOFSET);
		return 0;
	}

	va = uvm_km_alloc(kernel_map, endpa - pa, 0,
	    UVM_KMF_VAONLY | UVM_KMF_NOWAIT);
	if (va == 0)
		return ENOMEM;

	*bshp = va + (bpa & PGOFSET);

	for (; pa < endpa; pa += PAGE_SIZE, va += PAGE_SIZE) {
		pt_entry_t *ptep, npte;

		pmap_enter(pmap_kernel(), (vaddr_t)va, pa,
		    VM_PROT_READ|VM_PROT_WRITE, VM_PROT_READ|VM_PROT_WRITE);

		ptep = kvtopte(va);
		npte = *ptep & ~PG_CMASK;

		if ((flags & BUS_SPACE_MAP_CACHEABLE) == 0)
			npte |= PG_CI;
		else if (mmutype == MMU_68040)
			npte |= PG_CCB;

		*ptep = npte;
	}
	pmap_update(pmap_kernel());
	TBIAS();
	return 0;
}



void
bus_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh, bus_size_t size)
{
	vaddr_t	va, endva;
	paddr_t bpa;

	va = m68k_trunc_page(bsh);
	endva = m68k_round_page(((char *)bsh + size) - 1);
#ifdef DIAGNOSTIC
	if (endva < va)
		panic("%s: overflow", __func__);
#endif

	(void)pmap_extract(pmap_kernel(), va, &bpa);
	bpa += ((paddr_t)bsh & PGOFSET);

	/*
	 * Free the kernel virtual mapping.
	 */
	if (!bootm_free(va, endva - va)) {
		pmap_remove(pmap_kernel(), va, endva);
		pmap_update(pmap_kernel());
		uvm_km_free(kernel_map, va, endva - va, UVM_KMF_VAONLY);
	}

	/*
	 * Mark as free in the extent map.
	 */
	if (extent_free(iomem_ex, bpa, size, EX_NOWAIT | iomem_malloc_safe)
	    != 0) {
		printf("%s: pa 0x%lx, size 0x%lx\n", __func__, bpa, size);
		printf("%s: can't free region\n", __func__);
	}
}

/*
 * Get a new handle for a subregion of an already-mapped area of bus space.
 */
int
bus_space_subregion(bus_space_tag_t t, bus_space_handle_t memh,
    bus_size_t off, bus_size_t sz, bus_space_handle_t *mhp)
{

	*mhp = memh + off;
	return 0;
}


int
bus_space_alloc(bus_space_tag_t t, bus_addr_t rstart, bus_addr_t rend,
    bus_size_t size, bus_size_t alignment, bus_size_t boundary, int flags,
    bus_addr_t *bpap, bus_space_handle_t *bshp)
{
	u_long bpa;
	int error;

#ifdef DIAGNOSTIC
	/*
	 * Sanity check the allocation against the extent's boundaries.
	 * XXX: Since we manage the whole of memory in a single map,
	 *      this is nonsense for now! Brace it DIAGNOSTIC....
	 */
	/*
	if ((rstart + t->base) < iomem_ex->ex_start ||
	    (rend + t->base) > iomem_ex->ex_end)
		panic("%s: bad region start/end", __func__);
		*/
	if ((rstart + t) < iomem_ex->ex_start ||
		(rend + t) > iomem_ex->ex_end)
		panic("%s: bad region start/end", __func__);
#endif /* DIAGNOSTIC */

	/*
	 * Do the requested allocation.
	 */
	/*
	error = extent_alloc_subregion(iomem_ex, rstart + t->base,
	    rend + t->base, size, alignment, boundary,
	    EX_FAST | EX_NOWAIT | iomem_malloc_safe, &bpa);
	*/
	error = extent_alloc_subregion(iomem_ex, rstart + t,
		rend + t, size, alignment, boundary,
		EX_FAST | EX_NOWAIT | iomem_malloc_safe, &bpa);

	if (error != 0)
		return error;

	/*
	 * Map the bus physical address to a kernel virtual address.
	 */
	error = bus_mem_add_mapping(t, bpa, size, flags, bshp);
	if (error != 0) {
		if (extent_free(iomem_ex, bpa, size,
		    EX_NOWAIT | iomem_malloc_safe) != 0) {
			printf("%s: pa 0x%lx, size 0x%lx\n",
			    __func__, bpa, size);
			printf("%s: can't free region\n", __func__);
		}
	}

	*bpap = bpa;

	return error;
}


