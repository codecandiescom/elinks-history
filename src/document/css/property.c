/* CSS property info */
/* $Id: property.c,v 1.4 2004/01/18 15:25:47 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "document/css/property.h"
#include "document/css/value.h"


/* TODO: Use fastfind when we get a lot of properties. */
struct css_property_info css_property_info[] = {
	{ "background-color",	CSS_PT_BACKGROUND_COLOR, CSS_VT_COLOR,		css_parse_color_value },
	{ "color",		CSS_PT_COLOR,		 CSS_VT_COLOR,		css_parse_color_value },
	{ "font-style",		CSS_PT_FONT_STYLE,	 CSS_VT_FONT_ATTRIBUTE,	css_parse_font_attribute_value },
	{ "font-weight",	CSS_PT_FONT_WEIGHT,	 CSS_VT_FONT_ATTRIBUTE,	css_parse_font_attribute_value },
	{ "text-align",		CSS_PT_TEXT_ALIGN,	 CSS_VT_TEXT_ALIGN,	css_parse_text_align_value },

	{ NULL, CSS_PT_NONE, CSS_VT_NONE },
};
