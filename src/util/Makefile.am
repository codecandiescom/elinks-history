## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.16 2002/06/16 23:13:18 pasky Exp $

INCLUDES = \
 -I$(srcdir)/..

# The author of automake should be shot, quartered and forced to code in COBOL
# for the rest of his days for the shit he designed - anything forcing me to
# -I. -I$(srcdir) regardless if I want or not is just idiotic. Another totally
# dumb thing is that you can't just use @CONFIG_INCLUDE_SPEC@ in your own .am.

COMPILE = $(CC) @DEFS@ -I`echo $(CONFIG_HEADER) | sed "s,/[^/]*$$,,"` $(INCLUDES) \
$(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)

noinst_LIBRARIES = libutil.a
libutil_a_SOURCES = \
 base64.c \
 base64.h \
 blacklist.c \
 blacklist.h \
 conv.c \
 conv.h \
 encoding.c \
 encoding.h \
 error.c \
 error.h \
 hash.c \
 hash.h \
 memlist.c \
 memlist.h \
 secsave.c \
 secsave.h \
 string.c \
 string.h

