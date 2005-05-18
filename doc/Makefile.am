## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.60 2005/05/18 01:08:05 jonas Exp $

SUBDIRS = man

# TODO: perl.pod should be pod2ized during make install. --pasky

# XXX: manual.txt is the master document and must be first.
MANUAL_FILES = \
 manual.txt \
 \
 bookmarks.txt \
 ecmascript.txt \
 exmode.txt \
 faq.txt \
 installation.txt \
 introduction.txt \
 lua-scripting.txt \
 mailcap.txt \
 marks.txt \
 mime.txt \
 remote.txt \
 small.txt \
 tabs.txt \
 terminals.txt \
 urlshortcuts.txt

EXTRA_DIST = \
 README \
 $(MANUAL_FILES) \
 color-model.txt \
 events.txt \
 feedback.txt \
 hacking.txt \
 index.txt \
 perl.pod \
 tools/help2doc \
 tools/make-elinks-manpage \
 tools/make-elinkskeys-manpage

CLEANFILES = \
 *.man.txt \
 *.html.txt\
 elinks.html \
 elinkskeys.html \
 import-features.conf.* \
 manual.xml

ELINKS			= $(top_builddir)/src/elinks
KBDBIND			= $(top_srcdir)/src/config/kbdbind.c
FEATURES_CONF		= $(top_srcdir)/features.conf

HELP2DOC		= $(top_srcdir)/doc/tools/help2doc
IMPORT_FEATURES_CONF	= $(top_srcdir)/doc/tools/import-features.conf
MAKE_ELINKS_MANPAGE	= $(top_srcdir)/doc/tools/make-elinks-manpage
MAKE_ELINKSKEYS_MANPAGE	= $(top_srcdir)/doc/tools/make-elinkskeys-manpage

MAN_ELINKS_CONF		= $(top_srcdir)/doc/man/man5/elinks.conf.5
MAN_ELINKSKEYS		= $(top_srcdir)/doc/man/man5/elinkskeys.5
MAN_ELINKS		= $(top_srcdir)/doc/man/man1/elinks.1.in

%.man.xml: %.man.txt
	asciidoc -b docbook -d manpage $<

%.html: %.html.txt
	asciidoc -b css-embedded -d manpage -o $@ $<

manual.xml: $(MANUAL_FILES) import-features.conf.txt
	asciidoc -b docbook -d book -o $@ $<

import-features.conf.txt: $(FEATURES_CONF) $(IMPORT_FEATURES_CONF)
	$(IMPORT_FEATURES_CONF) > $@

%.1: %.man.xml
	xmlto man $<
	mv $@ $(MAN_ELINKS)

%.5: %.man.xml Makefile.am
	xmlto man $<
	sed -e 's/\\fI\\fR'\''/\\fI\\'\''\\fR/' < $@ > $(MAN_ELINKSKEYS)

elinks.%.txt: $(MAKE_ELINKS_MANPAGE)
	@if test ! -x "$(ELINKS)";						\
	then									\
		echo "There is no '$(ELINKS)' to update from.";			\
	else									\
		$(MAKE_ELINKS_MANPAGE) $@ $(ELINKS) > $@;			\
	fi

elinkskeys.%.txt: $(MAKE_ELINKSKEYS_MANPAGE)
	@if test ! -e "$(KBDBIND)";						\
	then									\
		echo "There is no '$(KBDBIND)' to update from.";		\
	else									\
		$(MAKE_ELINKSKEYS_MANPAGE) $< $(KBDBIND) > $@;			\
	fi

# elinkskeys.5
manpages: elinks.1
	@if test ! -x "$(ELINKS)";						\
	then									\
		echo "There is no '$(ELINKS)' to update from.";			\
	else									\
		sh $(HELP2DOC) --elinks=$(ELINKS) --elinksconf > $(MAN_ELINKS_CONF);	\
	fi

manual: manual.xml Makefile.am
	xmlto -o manual html $<

manual.html: $(MANUAL_FILES) import-features.conf.txt Makefile.am
	asciidoc -b css-embedded -d book -n manual.txt
