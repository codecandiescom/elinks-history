## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.23 2003/12/20 18:30:17 pasky Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS = auth file ftp http rewrite smb

noinst_LIBRARIES = libprotocol.a
libprotocol_a_SOURCES = \
 finger.c \
 finger.h \
 protocol.c \
 protocol.h \
 proxy.c \
 proxy.h \
 uri.c \
 uri.h \
 user.c \
 user.h
