## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.43 2005/01/08 21:45:28 jonas Exp $

SUBDIRS = Unicode config contrib doc intl po src test

EXTRA_DIST = \
 ABOUT-NLS \
 BUGS \
 Makefile.base \
 SITES \
 TODO \
 autogen.sh \
 features.conf

install-exec-hook:
	@echo "**************************************************************"
	@echo "If you are upgrading from Links or ELinks <= 0.4pre7, you may"
	@echo "want to check out INSTALL for some hints; ie. you may want to"
	@echo "make symlink 'links' pointing to 'elinks'."
	@echo "**************************************************************"


CLEANFILES = $(srcdir)/features.log
