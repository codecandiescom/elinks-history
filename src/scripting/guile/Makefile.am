## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.4 2005/01/13 14:14:50 jonas Exp $

include $(top_srcdir)/Makefile.base

EXTRA_DIST = README

INCLUDES += $(GUILE_CFLAGS)

noinst_LIBRARIES = libscriptingguile.a
libscriptingguile_a_SOURCES = \
 core.c \
 core.h \
 hooks.c \
 hooks.h

