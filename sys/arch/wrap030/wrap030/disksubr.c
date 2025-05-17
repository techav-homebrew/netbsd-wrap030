/*	$NetBSD: disksubr.c,v 1.28 2019/04/03 22:10:50 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ufs_disksubr.c	8.5 (Berkeley) 1/21/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: disksubr.c,v 1.28 2019/04/03 22:10:50 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disk.h>
#include <sys/disklabel.h>

#include <machine/endian.h>
#include <machine/endian_machdep.h>

/*
 * Attempt to read a disk label from a device using the indicated strategy
 * routine.  The label must be partly set up before this: secpercyl and
 * anything required in the strategy routine (e.g., sector size) must be
 * filled in before calling us.  Returns null on success and an error
 * string on failure.
 */
const char *
readdisklabel(dev_t dev, void (*strat)(struct buf *), struct disklabel *lp,
    struct cpu_disklabel *osdep)
{
	struct buf *bp;
	struct disklabel *dlp;
	const char *msg = NULL;
	int i;

	printf("readdisklabel(%p,%p,%p,%p)\r\n",dev,strat,lp,osdep);

	printf("readdisklabel() strat: %p; *strat: %p\r\n",strat,*strat);

	printf("readdisklabel() initial lp values:\r\n");
	printf("\tlp->d_secsize:\t%u\r\n",lp->d_secsize);
	printf("\tlp->secperunit:\t%u\r\n",lp->d_secperunit);
	printf("\tlp->d_npartitions:\t%u\r\n",lp->d_npartitions);

	if (lp->d_secsize == 0)
		lp->d_secsize = DEV_BSIZE;
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;
	if (lp->d_npartitions < RAW_PART + 1)
		lp->d_npartitions = RAW_PART + 1;
	for (i = 0; i < RAW_PART; i++) {
		lp->d_partitions[i].p_size = 0;
		lp->d_partitions[i].p_offset = 0;
	}
	if (lp->d_partitions[RAW_PART].p_size == 0)
		lp->d_partitions[RAW_PART].p_size = lp->d_secperunit;
	lp->d_partitions[RAW_PART].p_offset = 0;

	lp->d_partitions[0].p_size = lp->d_partitions[RAW_PART].p_size;
	lp->d_partitions[0].p_fstype = FS_BSDFFS;

	
	printf("readdisklabel() updated lp values:\r\n");
	printf("\tlp->d_secsize:\t%u\r\n",lp->d_secsize);
	printf("\tlp->secperunit:\t%u\r\n",lp->d_secperunit);
	printf("\tlp->d_npartitions:\t%u\r\n",lp->d_npartitions);

	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;
	bp->b_blkno = LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags |= B_READ;
	bp->b_cylinder = LABELSECTOR / lp->d_secpercyl;

	/* read disklabel sector from disk? */
	(*strat)(bp);

	if (biowait(bp))
	{
		msg = "I/O error";
	}
	else 
	{
		dlp = (struct disklabel *)bp->b_data;
		/* endian-swapping this will be fun ... */
		dlp->d_magic = bswap32(dlp->d_magic);
		dlp->d_type = bswap16(dlp->d_type);
		dlp->d_subtype = bswap16(dlp->d_subtype);

		dlp->d_secsize = bswap32(dlp->d_secsize);
		dlp->d_nsectors = bswap32(dlp->d_nsectors);
		dlp->d_ntracks = bswap32(dlp->d_ntracks);
		dlp->d_ncylinders = bswap32(dlp->d_ncylinders);
		dlp->d_secpercyl = bswap32(dlp->d_secpercyl);
		dlp->d_secperunit = bswap32(dlp->d_secperunit);

		dlp->d_sparespertrack = bswap16(dlp->d_sparespertrack);
		dlp->d_sparespercyl = bswap16(dlp->d_sparespercyl);

		dlp->d_acylinders = bswap32(dlp->d_acylinders);

		dlp->d_rpm = bswap16(dlp->d_rpm);
		dlp->d_interleave = bswap16(dlp->d_interleave);
		dlp->d_trackskew = bswap16(dlp->d_trackskew);
		dlp->d_cylskew = bswap16(dlp->d_cylskew);
		dlp->d_headswitch = bswap32(dlp->d_headswitch);
		dlp->d_trkseek = bswap32(dlp->d_trkseek);
		dlp->d_flags = bswap32(dlp->d_flags);
		dlp->d_magic2 = bswap32(dlp->d_magic2);
		dlp->d_checksum = bswap16(dlp->d_checksum);

		dlp->d_npartitions = bswap16(dlp->d_npartitions);
		dlp->d_bbsize = bswap32(dlp->d_bbsize);
		dlp->d_sbsize = bswap32(dlp->d_sbsize);

		for(int part = 0; part < MAXPARTITIONS; part++)
		{
			dlp->d_partitions[part].p_size = bswap32(dlp->d_partitions[part].p_size);
			dlp->d_partitions[part].p_offset = bswap32(dlp->d_partitions[part].p_offset);
			dlp->d_partitions[part].p_fsize = bswap32(dlp->d_partitions[part].p_fsize);
			dlp->d_partitions[part].p_cdsession = bswap32(dlp->d_partitions[part].p_cdsession);
			dlp->d_partitions[part].p_cpg = bswap16(dlp->d_partitions[part].p_cpg);
			dlp->d_partitions[part].p_sgs = bswap16(dlp->d_partitions[part].p_sgs);
		}

		printf("readdisklabel() read magic: %08x, %08x\r\n",dlp->d_magic,dlp->d_magic2);
		if(dlp->d_magic != DISKMAGIC || dlp->d_magic2 != DISKMAGIC)
		{
			if(msg == NULL)
			{
				msg = "no disk label";
			}
			else if(dlp->d_npartitions > MAXPARTITIONS || dkcksum(dlp) != 0)
			{
				msg = "disk label corrupted";
			}
			else
			{
				*lp = *dlp;
				msg = NULL;
			}
		}

		/* 
		for (dlp = (struct disklabel *)bp->b_data;
			dlp <= (struct disklabel *)((char *)bp->b_data + DEV_BSIZE - sizeof(*dlp));
			dlp = (struct disklabel *)((char *)dlp + sizeof(long))) 
		{
			printf("readdisklabel() read magic: %08x, %08x\r\n",dlp->d_magic,dlp->d_magic2);
			if (dlp->d_magic != DISKMAGIC || dlp->d_magic2 != DISKMAGIC) 
			{
				if (msg == NULL)
				{
					msg = "no disk label";
				}
			}
			else if (dlp->d_npartitions > MAXPARTITIONS || dkcksum(dlp) != 0)
			{
				msg = "disk label corrupted";
			}
			else 
			{
				*lp = *dlp;
				msg = NULL;
				break;
			}
		}
		*/
	}
	brelse(bp, 0);
	return msg;
}

/*
 * Write disk label back to device after modification.
 */
int
writedisklabel(dev_t dev, void (*strat)(struct buf *), struct disklabel *lp,
    struct cpu_disklabel *osdep)
{
	struct buf *bp;
	struct disklabel *dlp;
	int labelpart;
	int error = 0;

	labelpart = DISKPART(dev);
	if (lp->d_partitions[labelpart].p_offset != 0) {
		if (lp->d_partitions[0].p_offset != 0)
			return EXDEV;			/* not quite right */
		labelpart = 0;
	}
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = MAKEDISKDEV(major(dev), DISKUNIT(dev), labelpart);
	bp->b_blkno = LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags |= B_READ;
	(*strat)(bp);
	if ((error = biowait(bp)) != 0)
		goto done;
	for (dlp = (struct disklabel *)bp->b_data;
	    dlp <= (struct disklabel *)
	      ((char *)bp->b_data + lp->d_secsize - sizeof(*dlp));
	    dlp = (struct disklabel *)((char *)dlp + sizeof(long))) {
		if (dlp->d_magic == DISKMAGIC && dlp->d_magic2 == DISKMAGIC &&
		    dkcksum(dlp) == 0) {
			*dlp = *lp;
			bp->b_oflags &= ~(BO_DONE);
			bp->b_flags &= ~(B_READ);
			bp->b_flags |= B_WRITE;
			(*strat)(bp);
			error = biowait(bp);
			goto done;
		}
	}
	error = ESRCH;
 done:
	brelse(bp, 0);
	return error;
}
