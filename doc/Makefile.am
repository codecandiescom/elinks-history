## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.46 2005/05/14 20:53:18 jonas Exp $

SUBDIRS = man book

# TODO: perl.pod should be pod2ized during make install. --pasky

EXTRA_DIST = \
 README \
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
 tools/xml2rfc

CLEANFILES = \
 elinks.man.txt \
 elinks.html.txt\
 elinks.html \
 $(TMPHTML) \
 $(TMPBOOK)

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

update-manpages:
	@if test ! -x "$(ELINKS)";						\
	then									\
		echo "There is no '$(ELINKS)' to update from.";			\
	else									\
		sh $(HELP2DOC) --elinks=$(ELINKS) --elinksconf > $(MANCONF);	\
	fi

BOOK	 = $(top_srcdir)/doc/book/book.xml
TMPBOOK	 = $(top_builddir)/doc/book/.book.xml
TXTBOOK	 = $(top_srcdir)/doc/manual.txt
HTMLBOOK = $(top_srcdir)/doc/manual.html
TMPHTML  = $(top_srcdir)/doc/.manual.html
MANBOOK	 = $(top_srcdir)/doc/man/man1/elinksmanual.1
XML2RFC	 = $(top_srcdir)/doc/tools/xml2rfc

dist: update-manual
update-manual:
	@if test "x$(TCLSH)" != "x";						\
	then									\
		< $(BOOK) sed 's/\$$\(Revision: .*\) \$$/\1/' > $(TMPBOOK);	\
		echo "Updating $(TXTBOOK)... ";					\
		sh $(XML2RFC) $(TMPBOOK) $(TXTBOOK);				\
		echo "Updating $(HTMLBOOK)... ";				\
		sh $(XML2RFC) $(TMPBOOK) $(TMPHTML);				\
		< $(TMPHTML) sed -e 's/é/\&eacute;/g' -e 's/á/\&aacute;/g' > $(HTMLBOOK) \
		&& rm -f $(TMPHTML);						\
		echo "Updating $(MANBOOK)... ";					\
		sh $(XML2RFC) $(TMPBOOK) $(MANBOOK);				\
		rm -f $(TMPBOOK);						\
	else									\
		echo "You need tclsh installed and in your PATH to update the manual.";	\
	fi
