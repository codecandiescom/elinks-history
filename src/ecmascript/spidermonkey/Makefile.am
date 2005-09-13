## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.4 2005/09/13 22:28:06 pasky Exp $

include $(top_srcdir)/Makefile.base

INCLUDES += $(SPIDERMONKEY_CFLAGS)

# We don't want to expose this outside of /src/ecmascript/ at all, and we can't
# merge the whole library of these with libecmascript.a. So we only compile
# objects here and link them directly to libecmascript.a in ../Makefile.am.

# However, we apparently need to create the library in order to convince
# automake to compile the objects :/. We DON'T use libbackend.a ANYWHERE.

noinst_LIBRARIES = libbackend.a

libbackend_a_SOURCES = \
 document.c \
 document.h \
 form.c \
 form.h \
 location.c \
 location.h \
 util.h \
 window.c \
 window.h
