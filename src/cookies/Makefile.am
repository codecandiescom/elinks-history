## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.9 2002/04/23 20:47:29 zas Exp $

INCLUDES = \
 -I$(srcdir)/..

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
