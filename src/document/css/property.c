/* CSS property info */
/* $Id: property.c,v 1.3 2004/01/18 14:43:03 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "document/css/property.h"


/* TODO: Use fastfind when we get a lot of properties. */
struct css_property_info css_property_info[] = {
	{ "background-color",	CSS_PT_BACKGROUND_COLOR, CSS_VT_COLOR },
	{ "color",		CSS_PT_COLOR,		 CSS_VT_COLOR },
	{ "font-style",		CSS_PT_FONT_STYLE,	 CSS_VT_FONT_ATTRIBUTE },
	{ "font-weight",	CSS_PT_FONT_WEIGHT,	 CSS_VT_FONT_ATTRIBUTE },
	{ "text-align",		CSS_PT_TEXT_ALIGN,	 CSS_VT_TEXT_ALIGN },

	{ NULL, CSS_PT_NONE, CSS_VT_NONE },
};
