## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.9 2003/06/10 21:29:43 jonas Exp $

include $(top_srcdir)/doc/book/Makefile.doc

SUBDIRS = manpages

EXTRA_PROGRAMS = manual
manual_SOURCES = manual.xml
manual_HTMLFORMAT = manual.html

EXTRA_DIST = \
 $(manual_SOURCES) \
 Makefile.doc \
 README \
 TODO \
 config/* \
 elinksbool.dtd \
 features/* \
 info/* \
 intro/* \
 manual.xml

manual-html: $(manual_HTMLFORMAT)

manual: $(manual_HTMLFORMAT)
