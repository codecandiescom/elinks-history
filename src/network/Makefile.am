## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.6 2005/06/12 02:26:49 jonas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_SSL
ssldir = ssl
sslobj = $(ssldir)/ssl.o $(ssldir)/socket.o
endif

SUBDIRS = $(ssldir)

libnetwork_a_LIBADD = $(sslobj)

noinst_LIBRARIES = libnetwork.a
libnetwork_a_SOURCES = \
 connection.c \
 connection.h \
 dns.c \
 dns.h \
 socket.c \
 socket.h \
 state.c \
 state.h

