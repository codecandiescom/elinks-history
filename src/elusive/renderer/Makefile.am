## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.1 2002/12/31 01:49:40 pasky Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS = text

noinst_LIBRARIES = librenderer.a

librenderer_a_LIBADD = \
 text/renderer.o

librenderer_a_SOURCES = \
 renderer.c \
 renderer.h
