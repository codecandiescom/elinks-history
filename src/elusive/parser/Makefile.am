## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.3 2003/01/17 22:04:41 pasky Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS = html

noinst_LIBRARIES = libparser.a

libparser_a_LIBADD = \
 html/parser.o

libparser_a_SOURCES = \
 parser.c \
 parser.h \
 property.c \
 property.h \
 stack.c \
 stack.h \
 syntree.c \
 syntree.h
