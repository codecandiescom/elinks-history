## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.7 2003/10/18 13:14:24 jonas Exp $

SUBDIRS = man

EXTRA_DIST = \
 README \
 bookmarks.txt \
 elinks-lua.texi \
 feedback.txt \
 hacking.txt \
 mailcap.html \
 mime.html

update-manpages:
	@test -x $(top_srcdir)/src/elinks || echo "Build elinks first."
	sh tools/help2doc --elinksconf --elinks=$(top_srcdir)/src/elinks > man/man5/elinks.conf.5
