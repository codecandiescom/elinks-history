#!/bin/sh

echo acinclude.m4...
echo "dnl This is automatically generated from m4/ files! Do not modify!" > acinclude.m4
cat m4/*.m4 >> acinclude.m4

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

# On at least some FreeBSD boxes the libtool installation will cause configure
# to bail with the error
#
#	ltconfig: you must specify a host type if you use `--no-verify'
#
# Whether it is autoconf or libtool who is to blame I don't know. --jonas
case "`libtool --version`" in
	*1.3.4-freebsd-ports*)
		echo "fixing broken libtool"
		mv configure configure-
		cat configure- | sed -e 's/--no-verify//' > configure
		rm configure-
		chmod +x configure
	;;
esac
		
echo done
