## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.45 2004/05/04 07:55:48 jonas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_CSS
cssdir = css
cssobj = \
 css/apply.o \
 css/css.o \
 css/parser.o \
 css/property.o \
 css/scanner.o \
 css/stylesheet.o \
 css/value.o
endif

SUBDIRS = $(cssdir) html plain

noinst_LIBRARIES = libdocument.a

libdocument_a_LIBADD = \
 $(cssobj) \
 html/parser/forms.o \
 html/parser/link.o \
 html/parser/parse.o \
 html/parser/stack.o \
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

