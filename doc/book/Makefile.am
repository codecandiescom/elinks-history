## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.12 2003/06/24 20:13:03 jonas Exp $

include $(top_srcdir)/doc/book/Makefile.doc

SUBDIRS = manpages

EXTRA_PROGRAMS = book
book_SOURCES = book.xml
book_HTMLFORMAT = book.html

DISTCLEANFILES = config/cmdoptions.xml

EXTRA_DIST = \
 $(book_SOURCES) \
 Makefile.doc \
 README \
 TODO \
 book.xml \
 config/* \
 elinksbook.dtd \
 features/* \
 info/* \
 intro/*
