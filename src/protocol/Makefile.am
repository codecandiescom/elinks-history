## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.28 2004/05/21 11:39:54 jonas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_FINGER
fingerobj = finger.o
endif

ftpobj = ftp/ftp.o ftp/ftpparse.o

httpobj = http/blacklist.o http/codes.o http/http.o

if CONFIG_SSL
httpsobj = http/https.o
endif

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
 $(httpobj) \
 $(httpsobj) \
 $(rewriteobj) \
 $(smbobj)

libprotocol_a_SOURCES = \
 date.c \
 date.h \
 header.c \
 header.h \
 protocol.c \
 protocol.h \
 proxy.c \
 proxy.h \
 uri.c \
 uri.h \
 user.c \
 user.h
