/* $Id: feature.h,v 1.1 2003/12/31 19:13:20 pasky Exp $ */

#ifndef ELINKS__DOTDOT_FEATURE_H
#define ELINKS__DOTDOT_FEATURE_H

/* This file contains various compile-time configuration settings, which you
 * can adjust below. You can fine-tune the ELinks binary to include really only
 * what you want it to. There are still some things which are to be adjusted
 * only directly through the ./configure script arguments though, so check
 * ./configure --help out as well! */

/* For users:
 *
 * The "/" "*" and "*" "/" sequences start/end comments in this file. The
 * features are controlled by using the "#define FEATURE" command. If it is
 * commented out, it means the feature is disabled, otherwise it is enabled.
 * Therefore, if the default doesn't suit you, you can either comment it out
 * or remove the comment marks. */



/*** LEDs
 *
 * These are the tiny LED-like indicators, shown at the bottom-right of the
 * screen as [-----]. They are used for indication of various states, ie.
 * whether you are currently talking through a SSL-secured connection.
 *
 * Default: disabled */

/* #define USE_LEDS */


/*** Bookmarks
 *
 * ELinks has built-in hiearchic bookmarks support. Open the bookmarks manager
 * by pressing 's'. When bookmarks are enabled, also support for the internal
 * ELinks bookmarks format is always compiled in.
 *
 * Default: enabled */

#define BOOKMARKS


/*** XBEL Bookmarks
 *
 * ELinks also supports universal XML bookmarks format called XBEL, also
 * supported by ie. Galeon, various "always-have-my-bookmarks" websites and
 * number of universal bookmark convertors.
 *
 * Default: enabled if libexpat (required library) found */

#ifdef HAVE_LIBEXPAT
/* Comment out the following line if you want to always have this disabled: */
#undef HAVE_LIBEXPAT
#endif


#endif
