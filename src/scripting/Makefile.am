## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.9 2004/05/01 23:16:47 jonas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_GUILE
guiledir = guile
guileobj = $(guiledir)/core.o $(guiledir)/hooks.o
endif

if CONFIG_LUA
luadir = lua
luaobj = $(luadir)/core.o $(luadir)/hooks.o
endif

if CONFIG_PERL
perldir = perl
perlobj = $(perldir)/core.o $(perldir)/hooks.o
endif

SUBDIRS = $(guiledir) $(luadir) $(perldir)

EXTRA_DIST = TODO

noinst_LIBRARIES = libscripting.a

libscripting_a_LIBADD = \
 $(guileobj) \
 $(luaobj) \
 $(perlobj)

libscripting_a_SOURCES = \
 scripting.c \
 scripting.h

