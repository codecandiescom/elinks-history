## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.10 2003/06/10 23:47:29 jonas Exp $

include $(top_srcdir)/doc/book/Makefile.doc

SUBDIRS = manpages

EXTRA_PROGRAMS = book
book_SOURCES = book.xml
book_HTMLFORMAT = book.html

EXTRA_DIST = \
 $(book_SOURCES) \
 Makefile.doc \
 README \
 TODO \
 book.xml \
 config/* \
 elinksbool.dtd \
 features/* \
 info/* \
 intro/*
