## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.1 2005/01/18 10:29:40 jonas Exp $

include $(top_srcdir)/Makefile.base

EXTRA_DIST = README

INCLUDES += $(RUBY_CFLAGS)

noinst_LIBRARIES = libscriptingruby.a
libscriptingruby_a_SOURCES = \
 core.c \
 core.h \
 hooks.c \
 hooks.h \
 ruby.h

