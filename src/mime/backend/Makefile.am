## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.3 2003/10/31 12:57:22 jonas Exp $

include $(top_srcdir)/Makefile.base

# We don't want to expose this outside of src/mime/ at all, and we can't merge
# the whole library of these with libmime.a. So we only compile objects here
# and link them directly to libmime.a in ../Makefile.am.

# However, we apparently need to create the library in order to convience
# automake to compile the objects :/. We DON'T use libmimebackend.a ANYWHERE.

noinst_LIBRARIES = libmimebackend.a
libmimebackend_a_SOURCES = \
 common.c \
 common.h \
 default.c \
 default.h \
 mailcap.c \
 mailcap.h \
 mimetypes.c \
 mimetypes.h
