#!/bin/bash

# $Id: find_unused_translations.sh,v 1.1 2002/12/13 22:00:58 zas Exp $

# This script searchs for unused entries in ELinks translation files
# It then generates 3 files per .lng file
# .lng.unused contains unused entries
# .lng.new contains original file without unused entries
# .lng.patch is a patch to _remove_ unused entries from original file

#List all files where we found an entry (slower) ?
full=

#Generate a diff patch ?
genpatch=1

#Warning for newbies
if [ "$1" != "quiet" ]; then
echo '!!!Slow dangereous VIRUS script!!!'
echo
echo "1] Do you know what you're about to do ?"
echo "   Yes) press ENTER"
echo "   No ) press CTRL+C"
echo
read
echo "2] Are you an ELinks developper or a translator or something ?"
echo "   Yes) press ENTER"
echo "   No ) press CTRL+C"
echo
read
echo "3] Did you understand the two previous questions ?"
echo "   Yes) press ENTER and use quiet option next time."
echo "   No ) press CTRL+C"
echo
read
fi

echo "Starting..." >&2

#Find all source files
LIST=`find ../src \( -name '*.c' -o -name '*.h' \) -not -name 'lang_defs.h'`;
if [ ! "$LIST" ]; then exit 1; fi

#Find translations
TRANS=`cat english.lng | cut -d ',' -f1 | sort -u`
if [ ! "$TRANS" ]; then exit 1; fi

#Search if translation is used somewhere or not
A=$(
for i in $TRANS; do
	echo "Checking for $i" >&2
	found=0;
	for j in $LIST; do
		if [ "`fgrep $i $j`" ]; then
			found=1;
			echo " Found in $j" >&2
			if [ ! "$full"]; then
				break;
			fi
		fi;
	done ;
	if [ $found -eq 0 ]; then
		echo "$i";
		echo " NOT FOUND" >&2
	fi;
	echo >&2
done)

echo >&2

#Create a grep expression with not found entries
B=$(echo $A | sed 's/ \+$//; s/ /\\|/g')
if [ ! "$B" ]; then exit 1; fi

echo "Greping with $B" >&2

for i in *.lng; do
	echo " Generating $i.unused ..." >&2
	grep "$B" $i > $i.unused
	echo " Generating $i.new ..." >&2
	grep -v "$B" $i > $i.new
	if [ "$genpatch" ]; then
		echo " Generating $i.patch ..." >&2
		diff -u $i $i.new > $i.patch
	fi
done

echo >&2
echo "Unfound entries list: " >&2
echo $A | tr ' ' '\n' >&2
echo >&2
echo "All done." >&2

echo >&2
echo "Tip of the day: rm -f *.lng.unused *.lng.patch *.lng.new" >&2
