## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.10 2003/12/22 22:50:00 jonas Exp $

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
	@if test ! -x "$(ELINKS)";		\
	then							\
		echo "There is no '$(ELINKS)' to update from.";	\
	else							\
		sh tools/help2doc --elinks=$(ELINKS)		\
				  --elinksconf			\
				  > man/man5/elinks.conf.5;	\
	fi
