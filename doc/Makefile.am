## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.27 2005/05/02 12:04:09 jonas Exp $

SUBDIRS = man

# TODO: perl.pod should be pod2ized during make install. --pasky

EXTRA_DIST = \
 README \
 bookmarks.txt \
 color-model.txt \
 ecmascript.txt \
 elinks-lua.texi \
 events.txt \
 exmode.txt \
 faq.html \
 feedback.txt \
 hacking.txt \
 index.txt \
 introduction.txt \
 mailcap.txt \
 marks.txt \
 mime.html \
 perl.pod \
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

update-manual:
	@if test "x`which tclsh`" != "x";                                     \
	then                                                                  \
		echo -n "Updating manual.txt... ";			      \
		sh $(top_srcdir)/doc/tools/xml2rfc book/book.xml manual.txt;  \
		echo "done";				                      \
		echo -n "Updating manual.html... ";			      \
		sh $(top_srcdir)/doc/tools/xml2rfc book/book.xml manual.html; \
		echo "done";				                      \
	else                                                                  \
		echo "You need tclsh (in you PATH) to update the manual.";    \
	fi
