## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.1 2003/05/06 17:16:14 jonas Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS = backend

noinst_LIBRARIES = libmime.a

libmime_a_LIBADD = \
 backend/common.o \
 backend/default.o \
 backend/mailcap.o

libmime_a_SOURCES = \
 mime.c \
 mime.h
