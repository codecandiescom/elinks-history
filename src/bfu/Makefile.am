## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.25 2004/05/01 23:31:13 jonas Exp $

include $(top_srcdir)/Makefile.base

EXTRA_libbfu_a_SOURCES = leds.c leds.h

if CONFIG_LEDS
ledsobj = leds.o
endif

noinst_LIBRARIES = libbfu.a

libbfu_a_LIBADD = $(ledsobj)

libbfu_a_SOURCES = \
 button.c \
 button.h \
 checkbox.c \
 checkbox.h \
 dialog.c \
 dialog.h \
 group.c \
 group.h \
 hierbox.c \
 hierbox.h \
 hotkey.c \
 hotkey.h \
 inpfield.c \
 inpfield.h \
 inphist.c \
 inphist.h \
 listbox.c \
 listbox.h \
 listmenu.c \
 listmenu.h \
 menu.c \
 menu.h \
 msgbox.c \
 msgbox.h \
 style.c \
 style.h \
 text.c \
 text.h \
 widget.c \
 widget.h

