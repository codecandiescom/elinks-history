## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.47 2005/05/13 21:41:53 rrowan Exp $

SUBDIRS = Unicode config contrib debian doc intl po src test

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
	@if cat features.log | grep 'Lua' > /dev/null; then \
		echo "**************************************************************"; \
		echo "Lua 4 support in ELinks is obsolete.  If you are upgrading"; \
		echo "from ELinks <= 0.10.X, you must update $(CONFDIR)/hooks.lua."; \
		echo "**************************************************************"; \
	fi


CLEANFILES = $(srcdir)/features.log
MAINTAINERCLEANFILES = $(srcdir)/acinclude.m4
