## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.38 2004/01/18 15:41:03 jonas Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS = css html plain

noinst_LIBRARIES = libdocument.a

libdocument_a_LIBADD = \
 css/apply.o \
 css/parser.o \
 css/property.o \
 css/scanner.o \
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

