## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.1 2003/05/06 17:16:15 jonas Exp $

include $(top_srcdir)/Makefile.base

# We don't want to expose this outside of src/mime/ at all, and we can't
# merge the whole library of these with libbookmarks.a. So we only compile
# objects here and link them directly to libbookmarks.a in ../Makefile.am.

# However, we apparently need to create the library in order to convience
# automake to compile the objects :/. We DON'T use libbackend.a ANYWHERE.

noinst_LIBRARIES = libmimebackend.a
libmimebackend_a_SOURCES = \
 common.c \
 common.h \
 default.c \
 default.h \
 mailcap.c \
 mailcap.h
