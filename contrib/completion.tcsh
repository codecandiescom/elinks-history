#!/bin/tcsh
# Completions for Elinks.
# Contributed by Jonas Fonseca <fonseca@diku.dk>.
# $Id: completion.tcsh,v 1.3 2002/05/19 19:34:56 pasky Exp $

set elinksoptions = \
	(accesskey-enter accesskey-priority allow-special-files		\
	anonymous assume-codepage async-dns base-session color-dirs	\
	cookies-accept cookies-paranoid-security cookies-save		\
	cookies-resave default-fg default-link default-vlink		\
	default-mime-type download-dir download-utime dump dump-width	\
	format-cache-size form-submit-auto form-submit-confirm		\
	ftp.anonymous-password ftp-proxy h help http-bugs.allow-blacklist \
	http-bugs.bug-302-redirect http-bugs.bug-post-no-keepalive	\
	http-bugs.http10 http-proxy http-referer fake-referer		\
	enable-global-history keep-unhistory language lookup 		\
	max-connections max-connections-to-host memory-cache-size	\
	no-connect proxy-user proxy-passwd receive-timeout retries	\
	secure-save show-status-bar show-title-bar source 		\
	unrestartable-receive-timeout user-agent version)

set elinkslanguages = \
	(bulgarian catalan czech danish dutch english estonian finnish	\
	french galician german	greek hungarian	icelandic indonesian	\
	italian lithuanian polish romanian russian slovak spanish	\
	swedish turkish ukrainian)

complete {e,}links \
	c/-{-,}/'$elinksoptions'/					\
	n/-{-,}accesskey-enter/'(0 1)'/					\
	n/-{-,}accesskey-priority/'(0 1 2)'/				\
	n/-{-,}allow-special-files/'(0 1)'/				\
	n/-{-,}{async-dns,color-dirs}/'(0 1)'/				\
	n/-{-,}cookies-accept/'(0 1 2)'/				\
	n/-{-,}cookies-{paranoid-security,save,resave}/'(0 1)'/		\
	n/-{-,}default-{fg,link,vlink}/x:'<color|#rrggbb>'/		\
	n/-{-,}download-utime/'(0 1)'/					\
	n/-{-,}download-dir/d/						\
	n/-{-,}{enable-global-history,keep-unhistory}/'(0 1)'/		\
	n/-{-,}form-\*/'(0 1)'/						\
	n/-{-,}{ftp,http}-proxy/x:'<host:port>'/			\
	n/-{-,}http-referer/'(0 1 2 3)'/				\
	n/-{-,}http-bugs.\*/'(0 1)'/ 					\
	n/-{-,}language/'$elinkslanguages'/				\
	n/-{-,}{secure-save,show-status-bar,show-title-bar}/'(0 1)'/
