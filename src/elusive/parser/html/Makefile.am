## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.1 2002/12/29 13:40:29 pasky Exp $

include $(top_srcdir)/Makefile.base

# We don't want to expose this outside of elusive/parser/ at all, and we can't
# merge the whole library of these with libparser.a. So we only compile
# objects here and link them directly to libparser.a in ../Makefile.am.

# However, we apparently need to create the library in order to convience
# automake to compile the objects :/. We DON'T use libhtml.a ANYWHERE.

noinst_LIBRARIES = libhtml.a
libhtml_a_SOURCES = \
 parser.c \
 parser.h
