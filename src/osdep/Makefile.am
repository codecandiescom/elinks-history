## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.23 2003/10/28 00:07:31 pasky Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS = beos sysinfo

noinst_LIBRARIES = libosdep.a

libosdep_a_LIBADD = \
 beos/beos.o \
 beos/overrides.o

libosdep_a_SOURCES = \
 ascii.h \
 generic.h \
 newwin.c \
 newwin.h \
 os2.c \
 os2.h \
 osdep.c \
 osdep.h \
 riscos.c \
 riscos.h \
 system.h \
 unix.c \
 unix.h \
 win32.c \
 win32.h

