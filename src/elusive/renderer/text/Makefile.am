## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.1 2002/12/31 01:49:40 pasky Exp $

include $(top_srcdir)/Makefile.base

# We don't want to expose this outside of elusive/renderer/ at all, and we can't
# merge the whole library of these with librenderer.a. So we only compile
# objects here and link them directly to librenderer.a in ../Makefile.am.

# However, we apparently need to create the library in order to convience
# automake to compile the objects :/. We DON'T use libtext.a ANYWHERE.

noinst_LIBRARIES = libtext.a
libtext_a_SOURCES = \
 renderer.c \
 renderer.h
