## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.26 2003/10/31 19:54:09 jonas Exp $

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
 draw.c \
 draw.h \
 options.c \
 options.h \
 plain.c \
 plain.h \
 refresh.c \
 refresh.h \
 view.c \
 view.h

