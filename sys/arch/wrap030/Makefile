#	$NetBSD: Makefile,v 1.12 2008/10/25 22:27:37 apb Exp $

# Makefile for wrap030 tags file and boot blocks

# Find where m68k source files are for inclusion in tags
.include <../m68k/Makefile.inc>

TWRAP030=	../wrap030/tags
SWRAP030=	../wrap030/wrap030/*.[ch] ../wrap030/include/*.h \
	../wrap030/dev/*.[ch]
AWRAP030=	../wrap030/wrap030/*.s

# Directories in which to place tags links
DWRAP030=	dev wrap030 include

.include "../../kern/Make.tags.inc"

tags:
	-ctags -wdtf ${TWRAP030} ${SWRAP030} ${SM68K} ${COMM}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AWRAP030} ${AM68K} | \
	    ${TOOL_SED} -e \
		"s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
	    >> ${TWRAP030}
	sort -o ${TWRAP030} ${TWRAP030}

links:
	-for i in ${DWRAP030}; do \
	    cd $$i && rm -f tags; ln -s ../tags tags; done


SUBDIR=	compile include stand

.include <bsd.subdir.mk>
