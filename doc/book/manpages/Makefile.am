## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.4 2003/06/09 22:39:02 jonas Exp $

include $(top_srcdir)/doc/book/Makefile.doc

DOCBUILDDIR = $(top_srcdir)/doc/

EXTRA_PROGRAMS = manpages
manpages_SOURCES = elinks.1.xml elinkskeys.5.xml
manpages_MANFORMAT = elinks.1 elinkskeys.5
manpages_HTMLFORMAT = elinks.1.html elinkskeys.5.html

EXTRA_DIST = $(manpages_SOURCES)

manpages-man: $(manpages_MANFORMAT)

manpages-html: $(manpages_HTMLFORMAT)

manpages: $(manpages_HTMLFORMAT) $(manpages_MANFORMAT)
