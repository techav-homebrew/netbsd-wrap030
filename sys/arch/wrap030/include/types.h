/*	$NetBSD: types.h,v 1.22 2021/01/23 19:38:52 christos Exp $	*/

#ifndef _WRAP030_TYPES_H_
#define	_WRAP030_TYPES_H_

#include <m68k/types.h>

#define	__HAVE_DEVICE_REGISTER
#define	__HAVE_MM_MD_KERNACC
#define	__HAVE_BUS_SPACE_8

#if defined(_KERNEL)
#define	__HAVE_RAS
#endif

#endif /* !_WRAP030_TYPES_H_ */
