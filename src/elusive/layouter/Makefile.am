## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.1 2002/12/31 01:50:02 pasky Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS = syntree

noinst_LIBRARIES = liblayouter.a

liblayouter_a_LIBADD = \
 syntree/layouter.o

liblayouter_a_SOURCES = \
 layouter.c \
 layouter.h \
 rectangle.c \
 rectangle.h
