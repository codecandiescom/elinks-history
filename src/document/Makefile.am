## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.29 2003/10/31 22:37:16 pasky Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS = html

noinst_LIBRARIES = libdocument.a

libdocument_a_LIBADD = \
 html/frames.o \
 html/parser.o \
 html/renderer.o \
 html/tables.o

libdocument_a_SOURCES = \
 document.c \
 document.h \
 options.c \
 options.h \
 refresh.c \
 refresh.h \
 view.h

