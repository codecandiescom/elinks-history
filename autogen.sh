#!/bin/sh

export LANG=
export LC_CTYPE=
export LC_NUMERIC=
export LC_TIME=
export LC_COLLATE=
export LC_MONETARY=
export LC_MESSAGES=
export LC_PAPER=
export LC_NAME=
export LC_ADDRESS=
export LC_TELEPHONE=
export LC_MEASUREMENT=
export LC_IDENTIFICATION=
export LC_ALL=


echo acinclude.m4...
echo "dnl This is automatically generated from m4/ files! Do not modify!" > acinclude.m4
cat m4/*.m4 >> acinclude.m4

echo aclocal...
aclocal14

echo autoheader...
autoheader

echo automake...
automake14 -a -c

echo autoconf...
autoconf

echo config.cache...
rm -f config.cache

echo done
