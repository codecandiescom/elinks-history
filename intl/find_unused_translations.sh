#!/bin/sh

# $Id: find_unused_translations.sh,v 1.8 2002/12/16 10:08:25 zas Exp $

# This script lists unused translations and, if given the argument 'patch',
# generates <language>.lng.patch for each translation file to remove them.

echo "This script doesn't work... Either fix it or wait a new version ;)" && exit

echo 'Finding unused translations...'

LIST=$(find ../src -name '*.[ch]' -not -name 'lang_defs.h')
if [ -z "$LIST" ]
then
	echo 'No source code found!' >&2
	exit 1
fi

trap 'rm -f translations_used translations_unused' 0

# The sed/grep combination is ugly, but we must be careful of multiple
# translations on the same line.
sed -e 's/\(\<T_[0-9a-zA-Z_]\+\>\)/\
\1\
/g' ${LIST} | grep '^T_[0-9a-zA-Z_]\+$' | sort -u > translations_used

# I'd use a variable instead of a file, but newlines would not be preserved.
< english.lng cut -d, -f1 | sort -u | grep -v '^#' |
	comm -23 - translations_used > translations_unused

if ! grep . translations_unused 2>&1 >/dev/null
then
	echo 'No unused translations found.'
	exit
fi

echo 'Unused translations: ' >&2
< translations_unused sed -e 's/^/        /' >&2

if [ "$1" = 'patch' ]
then
	echo 'Generating english.lng.patch ...'
	sed -e "s/^\\($(< translations_unused tr '\n' ' ' | sed -e 's/ $/\
/;s/ /,\\\|/g')\\)/#\\1/" english.lng |
		diff -u english.lng - > english.lng.patch
fi

echo 'Done.'
