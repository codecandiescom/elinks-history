/* CSS property info */
/* $Id: property.c,v 1.2 2004/01/18 03:01:06 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "document/css/property.h"


/* TODO: Use fastfind when we get a lot of properties. */
struct css_property_info css_property_info[] = {
	{ "background-color",	CSS_DP_BACKGROUND_COLOR, CSS_DV_COLOR },
	{ "color",		CSS_DP_COLOR,		 CSS_DV_COLOR },
	{ "font-style",		CSS_DP_FONT_STYLE,	 CSS_DV_FONT_ATTRIBUTE },
	{ "font-weight",	CSS_DP_FONT_WEIGHT,	 CSS_DV_FONT_ATTRIBUTE },
	{ "text-align",		CSS_DP_TEXT_ALIGN,	 CSS_DV_TEXT_ALIGN },

	{ NULL, CSS_DP_NONE, CSS_DV_NONE },
};
