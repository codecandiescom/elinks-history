## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.22 2002/12/01 11:08:17 pasky Exp $

SUBDIRS = Unicode contrib intl src test

man_MANS = elinks.1 elinkskeys.5

EXTRA_DIST = \
 $(man_MANS) BUGS TODO SITES Makefile.base doc/*

install-exec-hook:
	@echo "**************************************************************"
	@echo "If you are upgrading from Links or ELinks <= 0.4pre7, you may"
	@echo "want to check out INSTALL for some hints; ie. you may want to"
	@echo "make symlink 'links' pointing to 'elinks'."
	@echo "**************************************************************"
