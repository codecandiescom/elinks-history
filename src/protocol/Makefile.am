## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.27 2004/05/10 23:12:55 jonas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_FINGER
fingerobj = finger.o
endif

ftpobj = ftp/ftp.o ftp/ftpparse.o

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

EXTRA_libprotocol_a_SOURCES = finger.c finger.h
libprotocol_a_LIBADD = \
 $(fingerobj) \
 $(ftpobj) \
 $(rewriteobj) \
 $(smbobj)

libprotocol_a_SOURCES = \
 protocol.c \
 protocol.h \
 proxy.c \
 proxy.h \
 uri.c \
 uri.h \
 user.c \
 user.h
