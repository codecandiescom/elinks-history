## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.2 2003/06/04 19:04:33 jonas Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS = backend

noinst_LIBRARIES = libmime.a

libmime_a_LIBADD = \
 backend/common.o \
 backend/default.o \
 backend/mailcap.o \
 backend/mimetypes.o

libmime_a_SOURCES = \
 mime.c \
 mime.h
