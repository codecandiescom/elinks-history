/* $Id: value.h,v 1.10 2004/01/18 17:20:12 jonas Exp $ */

#ifndef EL__DOCUMENT_CSS_VALUE_H
#define EL__DOCUMENT_CSS_VALUE_H

#include "document/css/property.h"
#include "document/css/scanner.h"

/* This is interface for the value parser. It is intended to be used only
 * internally inside of the CSS engine. */

/* This function takes a value of a specified type from the given scanner and
 * converts it to a reasonable {struct css_property}-ready form. */
/* It returns positive integer upon success, zero upon parse error, and moves
 * the string pointer to the byte after the value end. */
int css_parse_value(struct css_property_info *propinfo,
		    union css_property_value *value,
		    struct css_scanner *scanner);


/* Here come the css_property_value_parsers provided. */

/* Takes no parser_data. */
int css_parse_background_value(struct css_property_info *propinfo,
				union css_property_value *value,
				unsigned char **string);

/* Takes no parser_data. */
int css_parse_color_value(struct css_property_info *propinfo,
			  union css_property_value *value,
			  unsigned char **string);

/* Takes no parser_data. */
int css_parse_font_style_value(struct css_property_info *propinfo,
				union css_property_value *value,
				unsigned char **string);

/* Takes no parser_data. */
int css_parse_font_weight_value(struct css_property_info *propinfo,
				union css_property_value *value,
				unsigned char **string);

/* Takes no parser_data. */
int css_parse_text_align_value(struct css_property_info *propinfo,
				union css_property_value *value,
				unsigned char **string);

#endif
