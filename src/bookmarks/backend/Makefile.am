## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.1 2002/12/01 19:02:09 pasky Exp $

include $(top_srcdir)/Makefile.base

# We don't want to expose this outside of /src/bookmarks/ at all, and we can't
# merge the whole library of these with libbookmarks.a. So we only compile
# objects here and link them directly to libbookmarks.a in ../Makefile.am.

# However, we apparently need to create the library in order to convience
# automake to compile the objects :/. We don't use libbackend.a anywhere.

noinst_LIBRARIES = libbackend.a
libbackend_a_SOURCES = \
 common.c \
 common.h \
 default.c \
 default.h

