## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.33 2005/05/03 23:31:49 jonas Exp $

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
 small.txt

ELINKS=$(top_builddir)/src/elinks

update-manpages:
	@if test ! -x "$(ELINKS)";						\
	then									\
		echo "There is no '$(ELINKS)' to update from.";			\
	else									\
		sh $(top_srcdir)/doc/tools/help2doc --elinks=$(ELINKS)		\
				  --elinksconf					\
				  > $(top_srcdir)/doc/man/man5/elinks.conf.5;	\
	fi

BOOK	 = $(top_srcdir)/doc/book/book.xml
TMPBOOK	 = $(top_builddir)/doc/book/.book.xml
TXTBOOK	 = $(top_srcdir)/doc/manual.txt
HTMLBOOK = $(top_srcdir)/doc/manual.html
MANBOOK	 = $(top_srcdir)/doc/man/man1/elinksmanual.1
XML2RFC	 = $(top_srcdir)/doc/tools/xml2rfc

update-manual:
	@if test "x$(TCLSH)" != "x";						\
	then									\
		cat $(BOOK) | sed 's/\$$\(Revision: .*\) \$$/\1/' > $(TMPBOOK); \
		echo "Updating $(TXTBOOK)... ";					\
		sh $(XML2RFC) $(TMPBOOK) $(TXTBOOK);				\
		echo "Updating $(HTMLBOOK)... ";				\
		sh $(XML2RFC) $(TMPBOOK) $(HTMLBOOK);				\
		echo "Updating $(MANBOOK)... ";					\
		sh $(XML2RFC) $(TMPBOOK) $(MANBOOK);				\
		rm -f $(TMPBOOK);						\
	else									\
		echo "You need tclsh (in you PATH) to update the manual.";	\
	fi
