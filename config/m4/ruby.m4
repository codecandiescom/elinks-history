dnl Thank you very much Vim for this lovely ruby configuration
dnl The hitchhiked code is from Vim configure.in version 1.98


AC_DEFUN([EL_CONFIG_RUBY],
[
AC_MSG_CHECKING([for Ruby])

CONFIG_RUBY="no";

EL_SAVE_FLAGS

AC_ARG_ENABLE(ruby,
	[  --enable-ruby           enable Ruby support],
	[if test "$enableval" != no; then CONFIG_RUBY="yes"; fi])

AC_MSG_RESULT($CONFIG_RUBY)

if test "$CONFIG_RUBY" = "yes"; then

	AC_SUBST(elinks_cv_path_ruby)
	AC_PATH_PROG(elinks_cv_path_ruby, ruby)
	if test "x$elinks_cv_path_ruby" != "x"; then

		AC_MSG_CHECKING(Ruby version)
		if $elinks_cv_path_ruby -e 'VERSION >= "1.6.0" or exit 1' >/dev/null 2>/dev/null; then
			ruby_version=`$elinks_cv_path_ruby -e 'puts VERSION'`
			AC_MSG_RESULT($ruby_version)

			AC_MSG_CHECKING(for Ruby header files)
			rubyhdrdir=`$elinks_cv_path_ruby -r mkmf -e 'print Config::CONFIG[["archdir"]] || $hdrdir' 2>/dev/null`

			if test "X$rubyhdrdir" != "X"; then
				AC_MSG_RESULT($rubyhdrdir)
				RUBY_CFLAGS="-I$rubyhdrdir"
				rubylibs=`$elinks_cv_path_ruby -r rbconfig -e 'print Config::CONFIG[["LIBS"]]'`

				if test "X$rubylibs" != "X"; then
					RUBY_LIBS="$rubylibs"
				fi

				librubyarg=`$elinks_cv_path_ruby -r rbconfig -e 'print Config.expand(Config::CONFIG[["LIBRUBYARG"]])'`

				if test -f "$rubyhdrdir/$librubyarg"; then
					librubyarg="$rubyhdrdir/$librubyarg"

				else
					rubylibdir=`$elinks_cv_path_ruby -r rbconfig -e 'print Config.expand(Config::CONFIG[["libdir"]])'`
					if test -f "$rubylibdir/$librubyarg"; then
						librubyarg="$rubylibdir/$librubyarg"
					elif test "$librubyarg" = "libruby.a"; then
						dnl required on Mac OS 10.3 where libruby.a doesn't exist
						librubyarg="-lruby"
					else
						librubyarg=`$elinks_cv_path_ruby -r rbconfig -e "print '$librubyarg'.gsub(/-L\./, %'-L#{Config.expand(Config::CONFIG[\"libdir\"])}')"`
					fi
				fi

				if test "X$librubyarg" != "X"; then
					RUBY_LIBS="$librubyarg $RUBY_LIBS"
				fi

				rubyldflags=`$elinks_cv_path_ruby -r rbconfig -e 'print Config::CONFIG[["LDFLAGS"]]'`
				if test "X$rubyldflags" != "X"; then
					LDFLAGS="$rubyldflags $LDFLAGS"
				fi

				EL_CONFIG(CONFIG_RUBY, [Ruby])
				LIBS="$RUBY_LIBS $LIBS"
				CPPFLAGS="$CPPFLAGS $RUBY_CFLAGS"
				AC_SUBST(RUBY_CFLAGS)
				AC_SUBST(RUBY_LIBS)
			else
				AC_MSG_RESULT(not found, disabling Ruby)
			fi
		else
			AC_MSG_RESULT(too old; need Ruby version 1.6.0 or later)
		fi
	fi
fi

if test "$CONFIG_RUBY" != "yes"; then
	EL_RESTORE_FLAGS
fi
])
