## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.40 2004/05/12 15:08:09 jonas Exp $

SUBDIRS = Unicode contrib doc intl po src test

EXTRA_DIST = ABOUT-NLS BUGS TODO SITES Makefile.base autogen.sh features.conf m4/*

install-exec-hook:
	@echo "**************************************************************"
	@echo "If you are upgrading from Links or ELinks <= 0.4pre7, you may"
	@echo "want to check out INSTALL for some hints; ie. you may want to"
	@echo "make symlink 'links' pointing to 'elinks'."
	@echo "**************************************************************"


CLEANFILES = $(srcdir)/features.log
DISTCLEANFILES = $(srcdir)/acinclude.m4
