## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.24 2004/05/02 12:52:21 jonas Exp $

include $(top_srcdir)/Makefile.base

noinst_LIBRARIES = libdialogs.a

libdialogs_a_SOURCES = \
 document.c \
 document.h \
 download.c \
 download.h \
 edit.c \
 edit.h \
 info.c \
 info.h \
 menu.c \
 menu.h \
 options.c \
 options.h \
 status.c \
 status.h

EXTRA_libdialogs_a_SOURCES = exmode.c exmode.h

if CONFIG_EXMODE
exmodeobj = exmode.o
endif

libdialogs_a_LIBADD = $(exmodeobj)
