## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.11 2003/01/02 19:36:56 jonas Exp $

include $(top_srcdir)/Makefile.base

noinst_LIBRARIES = libcookies.a
libcookies_a_SOURCES = \
 cookies.c \
 cookies.h \
 parser.c \
 parser.h

TESTS = parsetst

EXTRA_PROGRAMS = parsetst

parsetst_SOURCES = \
 parser.h \
 parser.c \
 parsetst.c
parsetst_DEPENDENCIES = testutils
parsetst_LDADD = $(top_srcdir)/src/util/libutil.a
