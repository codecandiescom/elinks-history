## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.11 2003/06/15 15:10:49 pasky Exp $

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
 elinksbook.dtd \
 features/* \
 info/* \
 intro/*
