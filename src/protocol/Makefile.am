## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.24 2004/05/02 12:52:04 jonas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_SMB
smbdir = smb
smbobj = $(smbdir)/smb.o
endif

SUBDIRS = auth file ftp http rewrite $(smbdir)

noinst_LIBRARIES = libprotocol.a

libprotocol_a_LIBADD = $(smbobj)

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
