## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.36 2004/01/17 16:46:44 pasky Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS = css html plain

noinst_LIBRARIES = libdocument.a

libdocument_a_LIBADD = \
 css/apply.o \
 css/parser.o \
 css/value.o \
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

