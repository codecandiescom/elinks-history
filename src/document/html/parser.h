/* $Id: parser.h,v 1.92 2005/07/10 00:49:38 miciah Exp $ */

#ifndef EL__DOCUMENT_HTML_PARSER_H
#define EL__DOCUMENT_HTML_PARSER_H

#include "intl/charsets.h" /* unicode_val_T */
#include "util/align.h"
#include "util/color.h"
#include "util/lists.h"

struct document_options;
struct form_control;
struct frameset_desc;
struct html_context;
struct memory_list;
struct menu_item;
struct part;
struct session;
struct string;
struct terminal;
struct uri;

/* XXX: This is just terible - this interface is from 75% only for other HTML
 * files - there's lack of any well defined interface and it's all randomly
 * mixed up :/. */

enum format_attr {
	AT_BOLD = 1,
	AT_ITALIC = 2,
	AT_UNDERLINE = 4,
	AT_FIXED = 8,
	AT_GRAPHICS = 16,
	AT_SUBSCRIPT = 32,
	AT_SUPERSCRIPT = 64,
	AT_PREFORMATTED = 128,
};

struct text_attrib_style {
	enum format_attr attr;
	color_T fg;
	color_T bg;
};

struct text_attrib {
	struct text_attrib_style style;

	int fontsize;
	unsigned char *link;
	unsigned char *target;
	unsigned char *image;
	unsigned char *title;
	struct form_control *form;
	color_T clink;
	color_T vlink;
#ifdef CONFIG_BOOKMARKS
	color_T bookmark_link;
#endif
	color_T image_link;

	unsigned char *select;
	int select_disabled;
	unsigned int tabindex;
	unicode_val_T accesskey;

	unsigned char *onclick;
	unsigned char *ondblclick;
	unsigned char *onmouseover;
	unsigned char *onhover;
	unsigned char *onfocus;
	unsigned char *onmouseout;
	unsigned char *onblur;
};

/* This enum is pretty ugly, yes ;). */
enum format_list_flag {
	P_NONE = 0,

	P_NUMBER = 1,
	P_alpha = 2,
	P_ALPHA = 3,
	P_roman = 4,
	P_ROMAN = 5,

	P_STAR = 1,
	P_O = 2,
	P_PLUS = 3,

	P_LISTMASK = 7,

	P_COMPACT = 8,
};

struct par_attrib {
	enum format_align align;
	int leftmargin;
	int rightmargin;
	int width;
	int list_level;
	unsigned list_number;
	int dd_margin;
	enum format_list_flag flags;
	color_T bgcolor;
};

/* HTML parser stack mortality info */
enum html_element_type {
	/* Elements of this type can not be removed from the stack. This type
	 * is created by the renderer when formatting a HTML part. */
	ELEMENT_IMMORTAL,
	/* Elements of this type can only be removed by elements of the start
	 * type. This type is created whenever a HTML state is created using
	 * init_html_parser_state(). */
	/* The element has been created by*/
	ELEMENT_DONT_KILL,
	/* These elements can safely be removed from the stack by both */
	ELEMENT_KILLABLE,
	/* These elements not only cannot bear any other elements inside but
	 * any attempt to do so will cause them to terminate. This is so deadly
	 * that it affects even invisible elements. Ie. <title>foo<body>. */
	ELEMENT_WEAK,
};

struct html_element {
	LIST_HEAD(struct html_element);

	enum html_element_type type;

	struct text_attrib attr;
	struct par_attrib parattr;
	int invisible;
	unsigned char *name;
	int namelen;
	unsigned char *options;
	int linebreak;
	struct frameset_desc *frameset;

	/* For the needs of CSS engine. A wannabe bitmask. */
	enum html_element_pseudo_class {
		ELEMENT_LINK = 1,
		ELEMENT_VISITED = 2,
	} pseudo_class;
};

enum html_special_type {
	SP_TAG,
	SP_FORM,
	SP_CONTROL,
	SP_TABLE,
	SP_USED,
	SP_FRAMESET,
	SP_FRAME,
	SP_NOWRAP,
	SP_CACHE_CONTROL,
	SP_CACHE_EXPIRES,
	SP_REFRESH,
	SP_STYLESHEET,
	SP_COLOR_LINK_LINES,
	SP_SCRIPT,
};

/* Interface for the renderer */

void
init_html_parser(struct uri *uri, struct document_options *options,
		 unsigned char *start, unsigned char *end,
		 struct string *head, struct string *title,
		 void (*put_chars)(struct html_context *, unsigned char *, int),
		 void (*line_break)(struct html_context *),
		 void *(*special)(struct html_context *, enum html_special_type,
		                  ...));

void done_html_parser(void);
struct html_element *init_html_parser_state(enum html_element_type type, int align, int margin, int width, struct html_context *html_context);
void done_html_parser_state(struct html_element *element,
                            struct html_context *html_context);

/* Interface for the table handling */

int get_bgcolor(unsigned char *, color_T *);
void set_fragment_identifier(unsigned char *attr_name, unsigned char *attr,
                             struct html_context *html_context);
void add_fragment_identifier(struct part *, unsigned char *attr,
                             struct html_context *html_context);

/* Interface for the viewer */

int
get_image_map(unsigned char *head, unsigned char *pos, unsigned char *eof,
	      struct menu_item **menu, struct memory_list **ml, struct uri *uri,
	      unsigned char *target_base, int to, int def, int hdef);

/* For html/parser/forms.c,link.c,parse.c,stack.c */

/* Ensure that there are at least n successive line-breaks at the current
 * position, but don't add more than necessary to bring the current number
 * of successive line-breaks above n.
 *
 * For example, there should be two line-breaks after a <br>, but multiple
 * successive <br>'s warrant still only two line-breaks.  ln_break will be
 * called with n = 2 for each of multiple successive <br>'s, but ln_break
 * will only add two line-breaks for the entire run of <br>'s. */
void ln_break(int n, struct html_context *html_context);

#ifdef CONFIG_ECMASCRIPT
int do_html_script(unsigned char *attr, unsigned char *html, unsigned char *eof, unsigned char **end, struct html_context *html_context);
#endif

#endif
