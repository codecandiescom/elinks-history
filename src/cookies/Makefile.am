## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.10 2002/06/16 23:38:22 pasky Exp $

include $(top_srcdir)/Makefile.base

noinst_LIBRARIES = libcookies.a
libcookies_a_SOURCES = \
 cookies.c \
 cookies.h \
 parser.c \
 parser.h

noinst_PROGRAMS = parsetst
parsetst_SOURCES = \
 parser.h \
 parser.c \
 parsetst.c

# I'm not sure about this.. --pasky
# Compilation failure when enabled. --Zas
#parsetst_DEPENDENCIES = \
# ../util/libutil.a
