## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.11 2004/05/06 22:35:43 jonas Exp $

include $(top_srcdir)/Makefile.base

if ENABLE_NLS
gettextdir = gettext
endif

SUBDIRS = $(gettextdir)

noinst_LIBRARIES = libintl.a
libintl_a_SOURCES = \
 charsets.c \
 charsets.h \
 codepage.h \
 codepage.inc \
 entity.inc \
 uni_7b.inc
