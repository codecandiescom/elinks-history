#!/bin/sh

echo acinclude.m4...
cat m4/*.m4 > acinclude.m4

echo aclocal...
aclocal

echo autoheader...
autoheader

echo automake...
automake -a -c

echo autoconf...
autoconf

echo config.cache...
rm -f config.cache

echo done
