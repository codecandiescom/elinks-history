/* CSS property info */
/* $Id: property.c,v 1.1 2004/01/18 02:55:59 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "document/css/property.h"


#define CSS_PROPERTY(name, property, valtype) \
	{ name, sizeof(name) - 1, property, valtype }

/* TODO: Use fastfind when we get a lot of properties. */
struct css_property_info css_property_info[] = {
	CSS_PROPERTY("background-color", CSS_DP_BACKGROUND_COLOR, CSS_DV_COLOR),
	CSS_PROPERTY("color", CSS_DP_COLOR, CSS_DV_COLOR),
	CSS_PROPERTY("font-style", CSS_DP_FONT_STYLE, CSS_DV_FONT_ATTRIBUTE),
	CSS_PROPERTY("font-weight", CSS_DP_FONT_WEIGHT, CSS_DV_FONT_ATTRIBUTE),
	CSS_PROPERTY("text-align", CSS_DP_TEXT_ALIGN, CSS_DV_TEXT_ALIGN),

	CSS_PROPERTY("", CSS_DP_NONE, CSS_DV_NONE),
};
