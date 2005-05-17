## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.50 2005/05/17 16:16:31 jonas Exp $

SUBDIRS = man book

# TODO: perl.pod should be pod2ized during make install. --pasky

# XXX: manual.txt is the master document and must be first.
MANUAL_FILES = \
 manual.txt \
 \
 introduction.txt \
 bookmarks.txt \
 tabs.txt \
 marks.txt \
 urlshortcuts.txt \
 terminals.txt \
 mime.txt \
 mailcap.txt \
 remote.txt \
 exmode.txt

EXTRA_DIST = \
 README \
 $(MANUAL_FILES) \
 color-model.txt \
 ecmascript.txt \
 elinks-lua.texi \
 events.txt \
 faq.html \
 feedback.txt \
 hacking.txt \
 index.txt \
 manual.txt \
 manual.html \
 perl.pod \
 small.txt \
 tools/help2doc \
 tools/make-elinks-manpage \
 tools/make-elinkskeys-manpage \
 tools/xml2rfc

CLEANFILES = \
 *.man.txt \
 *.html.txt\
 elinks.html \
 manual.xml

ELINKS			= $(top_builddir)/src/elinks
KBDBIND			= $(top_srcdir)/src/config/kbdbind.c
HELP2DOC		= $(top_srcdir)/doc/tools/help2doc
MAKE_ELINKS_MANPAGE	= $(top_srcdir)/doc/tools/make-elinks-manpage
MAKE_ELINKSKEYS_MANPAGE	= $(top_srcdir)/doc/tools/make-elinkskeys-manpage
MAN_ELINKS_CONF		= $(top_srcdir)/doc/man/man5/elinks.conf.5
MAN_ELINKSKEYS		= $(top_srcdir)/doc/man/man5/elinkskeys.5
MAN_ELINKS		= $(top_srcdir)/doc/man/man1/elinks.1.in

%.man.xml: %.man.txt
	asciidoc -b docbook -d manpage $<

%.html: %.html.txt
	asciidoc -b css-embedded -d manpage -o $@ $<

manual.xml: $(MANUAL_FILES)
	asciidoc -b docbook -d book -o $@ $<

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

update-manpages: elinks.1 elinkskeys.5
	@if test ! -x "$(ELINKS)";						\
	then									\
		echo "There is no '$(ELINKS)' to update from.";			\
	else									\
		sh $(HELP2DOC) --elinks=$(ELINKS) --elinksconf > $(MAN_ELINKS_CONF);	\
	fi

update-manual: manual.xml
	xmlto -o book html $<
