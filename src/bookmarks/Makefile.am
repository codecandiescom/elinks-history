## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.10 2002/12/01 19:02:08 pasky Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS = backend

noinst_LIBRARIES = libbookmarks.a

libbookmarks_a_LIBADD = \
 backend/common.o \
 backend/default.o

libbookmarks_a_SOURCES = \
 bookmarks.c \
 bookmarks.h \
 dialogs.c \
 dialogs.h

