## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.48 2005/05/13 21:42:35 rrowan Exp $

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
	@if cat features.log | grep 'Lua' > /dev/null; then \
		echo "**************************************************************"; \
		echo "Lua 4 support in ELinks is obsolete.  If you are upgrading"; \
		echo "from ELinks <= 0.10.X, you must update $(CONFDIR)/hooks.lua."; \
		echo "**************************************************************"; \
	fi


CLEANFILES = $(srcdir)/features.log
MAINTAINERCLEANFILES = $(srcdir)/acinclude.m4
