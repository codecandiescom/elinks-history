## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v

include $(top_srcdir)/Makefile.base

EXTRA_DIST = TODO VERSION config.charset locale.alias plural.y ref-add.sed.in ref-del.sed.in

DEFS = -DLIBDIR=\"$(libdir)\" @DEFS@
COMPILE = $(CC) $(DEFS) -I`dirname $(CONFIG_HEADER)` $(INCLUDES) $(AM_CFLAGS) $(CFLAGS)

localedir = $(datadir)/locale

noinst_LIBRARIES = libgettext.a
libgettext_a_SOURCES = \
 bindtextdom.c \
 dcgettext.c \
 dcigettext.c \
 dcngettext.c \
 dgettext.c \
 dngettext.c \
 explodename.c \
 finddomain.c \
 gettext.c \
 gettext.h \
 gettextP.h \
 hash-string.h \
 intl-compat.c \
 l10nflist.c \
 libgettext.h \
 libgnuintl.h \
 libintl.c \
 libintl.h \
 loadinfo.h \
 loadmsgcat.c \
 localcharset.c \
 localealias.c \
 ngettext.c \
 plural.c \
 textdomain.c

charset.alias: config.charset
	$(SHELL) $(srcdir)/config.charset '@host@' > t-$@
	mv t-$@ $@

# FIXME: Building plural.c from plural.y on the fly doesn't work

YACC = @INTLBISON@ -y -d
YFLAGS = --name-prefix=gettext__
plural.c: plural.y
.y.c:
	$(YACC) $(YFLAGS) --output $@ $<
	rm -f $*.h

install-exec-local: charset.alias all
	@if test '@USE_INCLUDED_LIBINTL@' = yes; then \
	  $(mkinstalldirs) $(DESTDIR)$(libdir); \
	  temp=$(DESTDIR)$(libdir)/t-charset.alias; \
	  dest=$(DESTDIR)$(libdir)/charset.alias; \
	  if test -f $(DESTDIR)$(libdir)/charset.alias; then \
	    orig=$(DESTDIR)$(libdir)/charset.alias; \
	    sed -f ref-add.sed $$orig > $$temp; \
	    $(INSTALL_DATA) $$temp $$dest; \
	    rm -f $$temp; \
	  else \
	    if test @GLIBC21@ = no; then \
	      orig=charset.alias; \
	      sed -f ref-add.sed $$orig > $$temp; \
	      $(INSTALL_DATA) $$temp $$dest; \
	      rm -f $$temp; \
	    fi; \
	  fi; \
	  $(mkinstalldirs) $(DESTDIR)$(localedir); \
	  test -f $(DESTDIR)$(localedir)/locale.alias \
	    && orig=$(DESTDIR)$(localedir)/locale.alias \
	    || orig=$(srcdir)/locale.alias; \
	  temp=$(DESTDIR)$(localedir)/t-locale.alias; \
	  dest=$(DESTDIR)$(localedir)/locale.alias; \
	  sed -f ref-add.sed $$orig > $$temp; \
	  $(INSTALL_DATA) $$temp $$dest; \
	  rm -f $$temp; \
	else \
	  : ; \
	fi

uninstall-local:
	@if test '@USE_INCLUDED_LIBINTL@' = yes; then \
	  if test -f $(DESTDIR)$(libdir)/charset.alias; then \
	    temp=$(DESTDIR)$(libdir)/t-charset.alias; \
	    dest=$(DESTDIR)$(libdir)/charset.alias; \
	    sed -f ref-del.sed $$dest > $$temp; \
	    if grep '^# Packages using this file: $$' $$temp > /dev/null; then \
	      rm -f $$dest; \
	    else \
	      $(INSTALL_DATA) $$temp $$dest; \
	    fi; \
	    rm -f $$temp; \
	  fi; \
	  if test -f $(DESTDIR)$(localedir)/locale.alias; then \
	    temp=$(DESTDIR)$(localedir)/t-locale.alias; \
	    dest=$(DESTDIR)$(localedir)/locale.alias; \
	    sed -f ref-del.sed $$dest > $$temp; \
	    if grep '^# Packages using this file: $$' $$temp > /dev/null; then \
	      rm -f $$dest; \
	    else \
	      $(INSTALL_DATA) $$temp $$dest; \
	    fi; \
	    rm -f $$temp; \
	  fi; \
	else \
	  : ; \
	fi
