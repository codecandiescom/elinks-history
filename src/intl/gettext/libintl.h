/* $Id: libintl.h,v 1.2 2003/01/03 00:51:08 pasky Exp $ */

#ifndef EL__INTL_GETTEXT_LIBINTL_H
#define EL__INTL_GETTEXT_LIBINTL_H

/* This header file provides an interface between ELinks and GNU libintl. I was
 * astonished that there is no libintl.h but libgnuintl.h (and that name seemed
 * ugly ;-), and I also decided that it will be better to introduce a clean
 * interface instead of digging to libgnuintl.h too much. */

/* Contrary to a standard gettext interface, we pass destination charset and
 * language (in form of struct terminal) directly with each call, allowing to
 * easily multiplex between several terminals. */
/* XXX: And currently we ignore the terminal. Will be fixed soon. --pasky */

#include "intl/gettext/libgettext.h"

#if 0

/* TODO: Make use of the term. */
#define _(msg,term) gettext(msg)

/* no-op - just for marking */
#define N_(msg) gettext_noop(msg)

#endif

#endif
