## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.16 2004/05/21 11:39:54 jonas Exp $

include $(top_srcdir)/Makefile.base

EXTRA_DIST = README.timegm

if CONFIG_SSL
httpsobj = https.o
endif

noinst_LIBRARIES = libhttp.a

EXTRA_libhttp_a_SOURCES = https.c https.h
libhttp_a_LIBADD = $(httpsobj)

libhttp_a_SOURCES = \
 blacklist.c \
 blacklist.h \
 codes.c \
 codes.h \
 http.c \
 http.h
