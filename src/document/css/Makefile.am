## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.4 2004/01/17 16:01:48 pasky Exp $

include $(top_srcdir)/Makefile.base

# We don't want to expose this outside of /src/document/ at all, and we can't
# merge the whole library of these with libdocument.a. So we only compile
# objects here and link them directly to libdocument.a in ../Makefile.am.

# However, we apparently need to create the library in order to convience
# automake to compile the objects :/. We DON'T use libcss.a ANYWHERE.

noinst_LIBRARIES = libcss.a
libcss_a_SOURCES = \
 apply.c \
 apply.h \
 parser.c \
 parser.h \
 value.c \
 value.h

