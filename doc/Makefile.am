## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.8 2003/10/18 13:24:17 jonas Exp $

SUBDIRS = man

EXTRA_DIST = \
 README \
 bookmarks.txt \
 elinks-lua.texi \
 feedback.txt \
 hacking.txt \
 mailcap.html \
 mime.html

ELINKS=$(top_srcdir)/src/elinks

update-manpages:
	@if test ! -x "$(top_srcdir)/src/elinks";		\
	then							\
		echo "Build elinks first.";			\
	else							\
		sh tools/help2doc --elinksconf			\
				  --elinks=$(ELINKS)		\
				  > man/man5/elinks.conf.5;	\
	fi
