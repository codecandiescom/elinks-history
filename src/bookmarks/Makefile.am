## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.11 2002/12/10 22:52:15 pasky Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS = backend

noinst_LIBRARIES = libbookmarks.a

libbookmarks_a_LIBADD = \
 backend/common.o \
 backend/default.o \
 backend/xbel.o

libbookmarks_a_SOURCES = \
 bookmarks.c \
 bookmarks.h \
 dialogs.c \
 dialogs.h
