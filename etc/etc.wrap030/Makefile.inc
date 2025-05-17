#	$NetBSD: Makefile.inc,v 1.9 2008/03/18 04:20:37 lukem Exp $
#
#	etc.wrap030/Makefile.inc -- wrap030-specific etc Makefile targets
#

# If you change the list of distributed kernels, don't forget
# to update the release documentation in distrib/notes/common/contents

KERNEL_SETS=		WRAP030

BUILD_KERNELS=		INSTALL

INSTALLATION_DIRS+=	installation/wrap030

