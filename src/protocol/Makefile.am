## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.25 2004/05/02 13:30:15 jonas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_SMB
smbdir = smb
smbobj = $(smbdir)/smb.o
endif

if CONFIG_URI_REWRITE
rewritedir = rewrite
rewriteobj = $(rewritedir)/rewrite.o
endif

SUBDIRS = auth file ftp http $(rewritedir) $(smbdir)

noinst_LIBRARIES = libprotocol.a

libprotocol_a_LIBADD = $(rewriteobj) $(smbobj)

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
