## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.3 2003/06/09 22:11:42 jonas Exp $

include $(top_srcdir)/doc/book/Makefile.doc

EXTRA_PROGRAMS = manpages
EXTRA_manpages_SOURCES = elinks.1.xml elinkskeys.5.xml
manpages_DEPENDENCIES = elinks.1 elinkskeys.5
EXTRA_DIST = $(EXTRA_manpages_SOURCES)

DOCBUILDDIR = $(top_srcdir)/doc/

manpages: $(EXTRA_manpages_SOURCES) $(manpages_DEPENDENCIES)
