## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.8 2003/01/03 02:05:07 pasky Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS = gettext

# THIS IS WRONG (so what, automake won't let us do better) :
BUILT_SOURCES = language.inc lang_defs.h $(wildcard lng_*.inc)
# @#(*^! :
EXTRA_DIST = $(srcdir)/stamp-intl language.inc lang_defs.h $(wildcard lng_*.inc)

noinst_LIBRARIES = libintl.a
libintl_a_SOURCES = \
 charsets.c \
 charsets.h \
 codepage.h \
 codepage.inc \
 entity.inc \
 language.h \
 uni_7b.inc

DISTCLEANFILES = $(srcdir)/stamp-intl $(srcdir)/language.inc $(srcdir)/lang_defs.h $(wildcard $(srcdir)/lng_*.inc)

# Dream mode on
# We could lng_*.inc : lng_%.inc : $(top_srcdir)/%.lng and then rebuild those
# files individually, if we would only teach gen-intl to do so... ;-) --pasky
# Dream mode off

# If we wouldn't be wrapped thru stamp-intl here, we would call gen-intl for
# each of the targets here.

$(srcdir)/language.inc $(srcdir)/lang_defs.h $(srcdir)/lng_*.inc: $(srcdir)/stamp-intl

$(srcdir)/stamp-intl: $(top_srcdir)/intl/*.lng
	cd $(top_srcdir)/intl; ./gen-intl
