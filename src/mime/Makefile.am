## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.3 2003/12/31 12:53:40 pasky Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS = backend

noinst_LIBRARIES = libmime.a

libmime_a_LIBADD = \
 backend/common.o \
 backend/default.o \
 backend/mailcap.o \
 backend/mimetypes.o

libmime_a_SOURCES = \
 dialogs.c \
 dialogs.h \
 mime.c \
 mime.h
