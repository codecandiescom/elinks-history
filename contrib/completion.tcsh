#!/bin/tcsh
# Completing for ELinks.
# Contributed by Jonas Fonseca <fonseca@diku.dk>.

complete links \
	c/-/'(accesskey-enter accesskey-priority allow-special-files    \
	anonymous assume-codepage async-dns base-session color-dirs     \
	cookies-accept cookies-paranoid-security cookies-save           \
	cookies-resave default-fg default-link default-vlink            \
	default-mime-type download-dir download-utime dump dump-width   \
	format-cache-size form-submit-auto form-submit-confirm          \
	ftp.anonymous-password ftp-proxy h help http-bugs.allow-blacklist \
	http-bugs.bug-302-redirect http-bugs.bug-post-no-keepalive      \
	http-bugs.http10 http-proxy http-referer fake-referer           \
	enable-global-history keep-unhistory language lookup            \
	max-connections max-connections-to-host memory-cache-size       \
	no-connect proxy-user proxy-passwd receive-timeout retries      \
	secure-save show-status-bar show-title-bar source               \
	unrestartable-receive-timeout user-agent version)'/             \
	n/-accesskey-enter/'(0 1)'/                                     \
	n/-accesskey-priority/'(0 1 2)'/                                \
	n/-allow-special-files/'(0 1)'/                                 \
	n/-assume-codepage/'(ASCII ISO-8859-1 ISO-8859-2 ISO-8859-4     \
	ISO-8859-5 ISO-8859-7 ISO-8859-9 ISO-8859-13 ISO-8859-15        \
	ISO-8859-16 "Window$-1250" "Window$-1251" "Window$-1257" CP-437 \
	CP-737 CP-850 CP-852 CP-866 CP-1125 MacRoman-2000 Mac-latin-2   \
	KamenickyBrothers KOI8-R KOI8-U TCVN-5712 VISCII Unicode UTF-8)'/ \
	n/-{async-dns,color-dirs}/'(0 1)'/                              \
	n/-cookies-accept/'(0 1 2)'/                                    \
	n/-{cookies-paranoid-security,cookies-save,cookies-resave}/'(0 1)'/ \
	n/-{default-fg,default-link,default-vlink}/x:'<color|#rrggbb>'/ \
	n/-download-utime/'(0 1)'/                                      \
	n/-download-dir/d/                                              \
	n/-{enable-global-history,keep-unhistory}/'(0 1)'/              \
	n/-form-\*/'(0 1)'/                                             \
	n/-{ftp-proxy,http-proxy}/x:'<host:port>'/                      \
	n/-http-referer/'(0 1 2 3)'/                                    \
	n/-http-bugs.\*/'(0 1)'/                                        \
	n/-language/'(bulgarian catalan czech                           \
	danish dutch english estonian finnish french galician german    \
	greek hungarian icelandic indonesian italian lithuanian polish  \
	romanian russian slovak spanish swedish turkish ukrainian)'/    \
	n/-{secure-save,show-status-bar,show-title-bar}/'(0 1)'/
