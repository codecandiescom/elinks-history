## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.1 2002/12/31 01:50:02 pasky Exp $

include $(top_srcdir)/Makefile.base

# We don't want to expose this outside of elusive/layouter/ at all, and we can't
# merge the whole library of these with liblayouter.a. So we only compile
# objects here and link them directly to liblayouter.a in ../Makefile.am.

# However, we apparently need to create the library in order to convience
# automake to compile the objects :/. We DON'T use libsyntree.a ANYWHERE.

noinst_LIBRARIES = libsyntree.a
libsyntree_a_SOURCES = \
 layouter.c \
 layouter.h
