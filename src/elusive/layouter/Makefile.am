## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.2 2003/01/17 21:48:34 pasky Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS = syntree

noinst_LIBRARIES = liblayouter.a

liblayouter_a_LIBADD = \
 syntree/layouter.o

liblayouter_a_SOURCES = \
 box.c \
 box.h \
 layouter.c \
 layouter.h
