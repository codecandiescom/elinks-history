#!/bin/bash

# This script prints translations statistics for .po files
# existing in the current directory

echo "Translations statistics"
echo "Date: `date`"
echo

for i in *.po; do
		msgfmt --statistics -o /dev/null $i 2>&1 \
	| sed 's/^\([0-9]\+ \)[^0-9]*\([0-9]\+ \)\?[^0-9]*\([0-9]\+ \)\?[^0-9]*$/\1\2\3/g' \
	| awk '{ \
		tot = $1 + $2 + $3; \
		if (tot != 0) \
			printf "%8s %.02f%% (%3d/%3d untranslated)\n",\
			"'"$i"'", $1*100/tot, $2+$3, tot}' ;
done | sort -r -n -k2

echo

# $Id: gen_translations_stats.sh,v 1.2 2003/05/21 18:07:39 jonas Exp $ #
