## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.2 2003/06/09 20:13:47 jonas Exp $

include $(top_srcdir)/doc/book/Makefile.doc

EXTRA_PROGRAMS = manpages
EXTRA_manpages_SOURCES = elinks.1.xml elinkskeys.5.xml
EXTRA_DIST = $(EXTRA_manpages_SOURCES)

manpages: $(EXTRA_manpages_SOURCES)
	@echo "Looking good" $<
