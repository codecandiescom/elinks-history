## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.22 2004/12/21 23:38:34 jonas Exp $

SUBDIRS = man

EXTRA_DIST = \
 README \
 bookmarks.txt \
 color-model.txt \
 ecmascript.txt \
 elinks-lua.texi \
 events.txt \
 exmode.txt \
 feedback.txt \
 hacking.txt \
 index.txt \
 introduction.txt \
 mailcap.txt \
 marks.txt \
 mime.html \
 remote.txt \
 small.txt \
 tabs.txt

ELINKS=$(top_builddir)/src/elinks

update-manpages:
	@if test ! -x "$(ELINKS)";                                            \
	then                                                                  \
		echo "There is no '$(ELINKS)' to update from.";               \
	else                                                                  \
		sh $(top_srcdir)/doc/tools/help2doc --elinks=$(ELINKS)        \
				  --elinksconf                                \
				  > $(top_srcdir)/doc/man/man5/elinks.conf.5; \
	fi
