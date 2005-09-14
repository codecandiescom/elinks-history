## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.9 2005/09/14 22:52:56 pasky Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS=spidermonkey

INCLUDES += $(SPIDERMONKEY_CFLAGS)

noinst_LIBRARIES = libecmascript.a
libecmascript_a_LIBADD = \
 spidermonkey/document.o \
 spidermonkey/form.o \
 spidermonkey/location.o \
 spidermonkey/unibar.o \
 spidermonkey/window.o
libecmascript_a_SOURCES = \
 ecmascript.c \
 ecmascript.h \
 spidermonkey.c \
 spidermonkey.h
