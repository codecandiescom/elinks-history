## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.27 2004/02/03 19:20:02 zas Exp $

include $(top_srcdir)/Makefile.base

SUBDIRS = beos os2 riscos unix win32

noinst_LIBRARIES = libosdep.a

libosdep_a_LIBADD = \
 beos/beos.o \
 beos/overrides.o \
 os2/os2.o \
 riscos/riscos.o \
 unix/unix.o \
 win32/win32.o

libosdep_a_SOURCES = \
 ascii.h \
 generic.h \
 getifaddrs.c \
 getifaddrs.h \
 newwin.c \
 newwin.h \
 osdep.c \
 osdep.h \
 stub.c \
 stub.h \
 system.h

