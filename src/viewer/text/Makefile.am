## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.9 2004/05/13 00:19:44 jonas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_MARKS
marksobj = marks.o
endif

noinst_LIBRARIES = libviewer_text.a

EXTRA_libviewer_text_a_SOURCES = marks.c marks.h
libviewer_text_a_LIBADD = $(marksobj)

libviewer_text_a_SOURCES = \
 form.c \
 form.h \
 link.c \
 link.h \
 search.c \
 search.h \
 textarea.c \
 textarea.h \
 view.c \
 view.h \
 vs.c \
 vs.h

