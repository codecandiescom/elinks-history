## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.34 2004/01/17 01:26:57 pasky Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS = html plain

noinst_LIBRARIES = libdocument.a

libdocument_a_LIBADD = \
 html/css.o \
 html/frames.o \
 html/parser.o \
 html/renderer.o \
 html/tables.o \
 plain/renderer.o

libdocument_a_SOURCES = \
 docdata.c \
 docdata.h \
 document.c \
 document.h \
 options.c \
 options.h \
 refresh.c \
 refresh.h \
 renderer.c \
 renderer.h \
 view.h

