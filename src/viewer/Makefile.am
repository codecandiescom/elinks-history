## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.2 2005/06/12 18:42:40 jonas Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS = dump text

if CONFIG_MARKS
marksobj = text/marks.o
endif

noinst_LIBRARIES = libviewer.a
libviewer_a_LIBADD = \
 dump/dump.o \
 text/draw.o \
 text/form.o \
 text/link.o \
 $(marksobj) \
 text/search.o \
 text/textarea.o \
 text/view.o \
 text/vs.o

libviewer_a_SOURCES = \
 timer.c \
 timer.h

