## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.34 2003/05/21 02:02:55 jonas Exp $

SUBDIRS = Unicode contrib doc intl po src test

EXTRA_DIST = ABOUT-NLS BUGS TODO SITES Makefile.base autogen.sh m4/*

install-exec-hook:
	@echo "**************************************************************"
	@echo "If you are upgrading from Links or ELinks <= 0.4pre7, you may"
	@echo "want to check out INSTALL for some hints; ie. you may want to"
	@echo "make symlink 'links' pointing to 'elinks'."
	@echo "**************************************************************"


DISTCLEANFILES = acinclude.m4 $(srcdir)/acinclude.m4

# The aclocal.m4 dependency genereated by automake is broken - it depends on
# acinclude.m4 instead of $(srcdir)/acinclude.m4. We try to workaround it.
# The workaround sometimes generates a circular dependency and make complains
# then - you just have to survive that.

acinclude.m4: $(srcdir)/acinclude.m4
	@echo "dnl This is never used. Look at $(srcdir)/acinclude.m4." > acinclude.m4

$(srcdir)/acinclude.m4: $(srcdir)/m4/*.m4
	@echo "dnl This is automatically generated from m4/ files! Do not modify!" > $(srcdir)/acinclude.m4
	cat $(srcdir)/m4/*.m4 >> $(srcdir)/acinclude.m4
