## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.14 2004/01/17 01:29:07 pasky Exp $

include $(top_srcdir)/Makefile.base

# We don't want to expose this outside of /src/document/ at all, and we can't
# merge the whole library of these with libdocument.a. So we only compile
# objects here and link them directly to libdocument.a in ../Makefile.am.

# However, we apparently need to create the library in order to convience
# automake to compile the objects :/. We DON'T use libhtml.a ANYWHERE.

EXTRA_DIST = TODO

noinst_LIBRARIES = libhtml.a
libhtml_a_SOURCES = \
 css.c \
 css.h \
 frames.c \
 frames.h \
 parser.c \
 parser.h \
 renderer.c \
 renderer.h \
 tables.c \
 tables.h

