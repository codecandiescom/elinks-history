## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.5 2004/11/14 21:32:18 jonas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_SSL_DIGEST
digestobj = digest.o
endif

EXTRA_libauth_a_SOURCES = digest.c digest.h
libauth_a_LIBADD = $(digestobj)

noinst_LIBRARIES = libauth.a
libauth_a_SOURCES = \
 auth.h \
 auth.c \
 dialogs.h \
 dialogs.c
