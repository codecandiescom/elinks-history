## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.1 2002/12/29 13:40:29 pasky Exp $

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
 syntree.c \
 syntree.h
