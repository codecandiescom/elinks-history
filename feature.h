/* $Id: feature.h,v 1.2 2003/12/31 21:26:15 jonas Exp $ */

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


/*** MIME
 *
 * ELinks uses a MIME system for determining the content type of documents and
 * configuring programs for external handling. By default the option system can
 * be used to configure how media types are handled. More info about how to set
 * up the MIME handling using the option system can be found in the
 * doc/mime.html file.
 *
 * Below are listed additional ways to do it. */

/*** Mailcap
 *
 * Mailcap files describe what program - on the local system - can be used to
 * handle a media type. The file format is defined in RFC 1524 and more info
 * includingexamples can be found in the doc/mailcap.html file.
 *
 * Default: enabled */

#define MAILCAP

/*** Mimetypes files
 *
 * Mimetypes files can be used to specify the relation between media types and
 * file extensions.
 *
 * Default: enabled */

#define MIMETYPES


/*** 256 colors in terminals
 *
 * Define to add support for using 256 colors in terminals. Note that it
 * requires a capable terminal emulator such as:
 *
 * - Thomas Dickey's XTerm, version 111 or later (check which version you have
 *   with xterm -version) compiled with --enable-256-color.
 *
 * - Recent versions of PuTTY also has minimal support of 256 colors.
 ø 
 * When enabled the memory usage is increased even when running in mono and 16
 * color mode.
 * 
 * Default: disabled */

/* #define USE_256_COLORS */

#endif
