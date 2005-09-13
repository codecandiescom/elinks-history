## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.5 2005/09/13 21:52:31 pasky Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS=spidermonkey

INCLUDES += $(SPIDERMONKEY_CFLAGS)

noinst_LIBRARIES = libecmascript.a
libecmascript_a_LIBADD = \
 spidermonkey/window.o
libecmascript_a_SOURCES = \
 ecmascript.c \
 ecmascript.h \
 spidermonkey.c \
 spidermonkey.h
