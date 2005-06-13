## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.26 2005/06/13 00:52:14 jonas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_INTERLINK
interlinkobj = interlink.o
endif

EXTRA_libmain_a_SOURCES = \
 interlink.c \
 interlink.h

libmain_a_LIBADD = \
 $(interlinkobj)

noinst_LIBRARIES = libmain.a
libmain_a_SOURCES = \
 event.c \
 event.h \
 main.c \
 main.h \
 module.c \
 module.h \
 select.c \
 select.h \
 timer.c \
 timer.h \
 version.c \
 version.h

