## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.3 2005/01/13 14:14:51 jonas Exp $

include $(top_srcdir)/Makefile.base

INCLUDES += $(PERL_CFLAGS)

noinst_LIBRARIES = libscriptingperl.a
libscriptingperl_a_SOURCES = \
 core.c \
 core.h \
 hooks.c \
 hooks.h \
 perl.h
