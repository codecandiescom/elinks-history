## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.43 2004/08/14 21:30:24 miciah Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_DEBUG
debugobj = memdebug.o
endif

if CONFIG_SMALL
else
fastfindobj = fastfind.o
endif

if CONFIG_CSS
scannerobj = scanner.o
endif

noinst_LIBRARIES = libutil.a

EXTRA_libutil_a_SOURCES = \
 fastfind.c \
 fastfind.h \
 memdebug.c \
 memdebug.h \
 scanner.c \
 scanner.h

libutil_a_LIBADD = \
 $(debugobj) \
 $(fastfindobj) \
 $(scannerobj)

libutil_a_SOURCES = \
 base64.c \
 base64.h \
 box.h	\
 color.c \
 color.h \
 color.inc \
 color_s.inc \
 conv.c \
 conv.h \
 error.c \
 error.h \
 file.c \
 file.h \
 hash.c \
 hash.h \
 lists.h \
 math.h \
 memlist.c \
 memlist.h \
 memory.c \
 memory.h \
 object.h \
 profile.h \
 secsave.c \
 secsave.h \
 snprintf.c \
 snprintf.h \
 string.c \
 string.h \
 ttime.c \
 ttime.h \
 types.h
