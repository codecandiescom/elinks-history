## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.11 2003/12/22 22:53:25 pasky Exp $

SUBDIRS = man

EXTRA_DIST = \
 README \
 bookmarks.txt \
 elinks-lua.texi \
 feedback.txt \
 hacking.txt \
 mailcap.html \
 mime.html

ELINKS=$(top_builddir)/src/elinks

update-manpages:
	@if test ! -x "$(ELINKS)";				\
	then							\
		echo "There is no '$(ELINKS)' to update from.";	\
	else							\
		sh $(top_srcdir)/doc/tools/help2doc --elinks=$(ELINKS)		\
				  --elinksconf			\
				  > $(top_builddir)/doc/man/man5/elinks.conf.5;	\
	fi
