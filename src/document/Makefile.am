## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.31 2003/11/10 21:29:29 jonas Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS = html plain

noinst_LIBRARIES = libdocument.a

libdocument_a_LIBADD = \
 html/frames.o \
 html/parser.o \
 html/renderer.o \
 html/tables.o \
 plain/renderer.o

libdocument_a_SOURCES = \
 document.c \
 document.h \
 options.c \
 options.h \
 refresh.c \
 refresh.h \
 renderer.c \
 renderer.h \
 view.h

