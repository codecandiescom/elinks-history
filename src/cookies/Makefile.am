## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.8 2002/04/23 08:06:21 pasky Exp $

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
parsetst_DEPENDENCIES = \
 ../util/libutil.a
