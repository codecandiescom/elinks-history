## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.6 2005/01/13 14:14:51 jonas Exp $

include $(top_srcdir)/Makefile.base

EXTRA_DIST = TODO

INCLUDES += $(GNUTLS_CFLAGS) $(OPENSSL_CFLAGS)

noinst_LIBRARIES = libssl.a
libssl_a_SOURCES = \
 connect.c \
 connect.h \
 ssl.c \
 ssl.h

