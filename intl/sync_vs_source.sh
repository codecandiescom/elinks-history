#!/bin/sh
# $Id: sync_vs_source.sh,v 1.1 2002/12/16 15:06:18 zas Exp $

# This script recreates an english.lng file matching current source code:
# Entries used in source code and present in original english.lng file will be
# uncommented/commented accordingly with sources. Comment prefix is #.
# Entries existing in english.lng but not found in sources, will be commented
# using ###? prefix.
# Entries found in sources but not existing in english.lng will be created as
# comments using ##prefix.
# Order of entries is preserved.

echo 'Searching sources...' >&2


LIST=$(find ../src -name '*.[ch]' -not -name 'lang_defs.h')
if [ -z "$LIST" ]
then
	echo 'No source code found!' >&2
	exit 1
fi

echo 'Generating patch...' >&2
(
for i in ${LIST}; do
	cat $i;
done
) | perl sync_vs_source.pl | diff -u english.lng - > english.lng.patch

cat english.lng.patch

echo 'Done.' >&2
