## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.34 2004/08/14 07:53:14 jonas Exp $

include $(top_srcdir)/Makefile.base

authobj = auth/auth.o auth/dialogs.o

if CONFIG_CGI
cgiobj = file/cgi.o
endif

if CONFIG_DATA
datasrc = data.c data.h
dataobj = data.o
endif

fileobj = file/file.o

if CONFIG_FINGER
fingerobj = finger.o
endif

ftpobj = ftp/ftp.o ftp/ftpparse.o

httpobj = http/blacklist.o http/codes.o http/http.o

if CONFIG_NNTP
nntpdir = nntp
nntpobj = $(nntpdir)/codes.o $(nntpdir)/connection.o $(nntpdir)/nntp.o $(nntpdir)/response.o
endif

if CONFIG_SMB
smbdir = smb
smbobj = $(smbdir)/smb.o
endif

if CONFIG_URI_REWRITE
rewritedir = rewrite
rewriteobj = $(rewritedir)/rewrite.o
endif

SUBDIRS = auth file ftp http $(nntpdir) $(rewritedir) $(smbdir)

noinst_LIBRARIES = libprotocol.a

EXTRA_libprotocol_a_SOURCES = $(datasrc) finger.c finger.h
libprotocol_a_LIBADD = \
 $(authobj) \
 $(dataobj) \
 $(cgiobj) \
 $(fileobj) \
 $(fingerobj) \
 $(ftpobj) \
 $(httpobj) \
 $(nntpobj) \
 $(rewriteobj) \
 $(smbobj)

libprotocol_a_SOURCES = \
 about.c \
 about.h \
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
