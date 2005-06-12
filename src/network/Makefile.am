## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.9 2005/06/12 22:22:45 jonas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_SSL
ssldir = ssl
sslobj = $(ssldir)/ssl.o $(ssldir)/socket.o
endif

if CONFIG_INTERLINK
interlinkobj = interlink.o
endif

SUBDIRS = $(ssldir)

EXTRA_libnetwork_a_SOURCES = \
 interlink.c \
 interlink.h

libnetwork_a_LIBADD = \
 $(interlinkobj) \
 $(sslobj)

noinst_LIBRARIES = libnetwork.a
libnetwork_a_SOURCES = \
 connection.c \
 connection.h \
 dns.c \
 dns.h \
 interlink.c \
 interlink.h \
 progress.c \
 progress.h \
 socket.c \
 socket.h \
 state.c \
 state.h

