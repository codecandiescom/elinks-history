## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.14 2004/09/29 23:18:17 jonas Exp $

SUBDIRS = man

EXTRA_DIST = \
 README \
 bookmarks.txt \
 color-model.txt \
 ecmascript.texi \
 elinks-lua.texi \
 events.txt \
 feedback.txt \
 hacking.txt \
 mailcap.html \
 mime.html \
 small.txt

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
