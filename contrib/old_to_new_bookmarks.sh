#!/bin/sh
# ELinks old bookmarks format to new format converter.

# WARNING: Close all ELinks sessions before running this script.
# This script converts ELinks bookmarks file with '|' as separator to new
# bookmarks format where separator is '\t'. It saves old file to
# ~/.links/bookmarks.with_pipes. --Zas


if [ ! -e ~/.links/bookmarks ]; then
	echo "~/.links/bookmarks does not exist !"
	exit 1
fi

if [ -e ~/.links/bookmarks.with_pipes ]; then
	echo "It seems you already ran this script."
	echo "Remove ~/.links/bookmarks.with_pipes to force execution."
	exit 1
fi
 
cat ~/.links/bookmarks | tr '|' '\t' > ~/.links/bookmarks.with_tabs \
&& cp -f ~/.links/bookmarks ~/.links/bookmarks.with_pipes \
&& mv -f ~/.links/bookmarks.with_tabs ~/.links/bookmarks && \
echo -e "Bookmarks file converted.\nOld file was saved as
~/.links/bookmarks.with_pipes." && exit 0

echo "Conversion failure"
exit 1
