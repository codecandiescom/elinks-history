## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.2 2003/01/01 18:58:13 pasky Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS = html

noinst_LIBRARIES = libparser.a

libparser_a_LIBADD = \
 html/parser.o

libparser_a_SOURCES = \
 attrib.c \
 attrib.h \
 parser.c \
 parser.h \
 stack.c \
 stack.h \
 syntree.c \
 syntree.h
