## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.14 2005/01/13 14:14:51 jonas Exp $

include $(top_srcdir)/Makefile.base

EXTRA_DIST = TODO

INCLUDES += $(LUA_CFLAGS)

noinst_LIBRARIES = libscriptinglua.a
libscriptinglua_a_SOURCES = \
 core.c \
 core.h \
 hooks.c \
 hooks.h \
 lua.h

