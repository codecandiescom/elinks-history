#!/bin/sh

# $Id: find_unused_translations.sh,v 1.4 2002/12/15 00:48:59 zas Exp $

# This script lists unused translations and, if given the argument 'patch',
# generates <language>.lng.patch for each translation file to remove them.

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

[ "$1" = 'patch' ] && for lang in *.lng
do
	echo Generating ${lang}.patch ...
	grep -vwF "$(cat translations_unused)" ${lang} |
		diff -u ${lang} - > ${lang}.patch
done

if [ "$1" = 'simple' ]; then
	E=$(echo $(cat translations_unused) | sed 's/ /\\\|/g')
	sed "s/^\($E\),/#\1/" english.lng |
		diff -u english.lng - > english.lng.patch


fi


echo 'Done.'
