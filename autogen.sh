#!/bin/sh

echo aclocal...
aclocal -I config/m4

echo autoheader...
autoheader

echo automake...
automake -a -c

echo autoconf...
autoconf

echo config.cache...
rm -f config.cache

echo done
