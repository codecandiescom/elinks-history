## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.31 2003/01/04 00:58:29 pasky Exp $

SUBDIRS = Unicode contrib doc po src test

man_MANS = elinks.1 elinkskeys.5 elinks.conf.5

EXTRA_DIST = \
 $(man_MANS) ABOUT-NLS BUGS TODO SITES Makefile.base autogen.sh m4/*

install-exec-hook:
	@echo "**************************************************************"
	@echo "If you are upgrading from Links or ELinks <= 0.4pre7, you may"
	@echo "want to check out INSTALL for some hints; ie. you may want to"
	@echo "make symlink 'links' pointing to 'elinks'."
	@echo "**************************************************************"


DISTCLEANFILES = acinclude.m4 $(srcdir)/acinclude.m4

# The aclocal.m4 dependency genereated by automake is broken - it depends on
# acinclude.m4 instead of $(srcdir)/acinclude.m4. We try to workaround it.

acinclude.m4: $(srcdir)/acinclude.m4
	@echo "dnl This is never used. Look at $(srcdir)/acinclude.m4." > acinclude.m4

$(srcdir)/acinclude.m4: $(srcdir)/m4/*.m4
	@echo "dnl This is automatically generated from m4/ files! Do not modify!" > $(srcdir)/acinclude.m4
	cat $(srcdir)/m4/*.m4 >> $(srcdir)/acinclude.m4
