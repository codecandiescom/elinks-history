/* $Id: parser.h,v 1.54 2003/12/27 01:26:20 miciah Exp $ */

#ifndef EL__DOCUMENT_HTML_PARSER_H
#define EL__DOCUMENT_HTML_PARSER_H

#include "bfu/style.h"
#include "util/color.h"
#include "util/lists.h"

struct document_options;
struct form_control;
struct frameset_desc;
struct list_head;
struct memory_list;
struct menu_item;
struct session;
struct string;
struct terminal;


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
};

struct text_attrib_beginning {
	enum format_attr attr;
	color_t fg;
	color_t bg;
};

struct text_attrib {
	/* Should match struct text_attrib_beginning fields
	 * FIXME: can we use a field of struct text_attrib_beginning type ?
	 */
	enum format_attr attr;
	color_t fg;
	color_t bg;

	int fontsize;
	unsigned char *link;
	unsigned char *target;
	unsigned char *image;
	unsigned char *title;
	struct form_control *form;
	color_t clink;
	color_t vlink;
	unsigned char *href_base;
	unsigned char *target_base;
	unsigned char *select;
	int select_disabled;
	unsigned int tabindex;
	long accesskey;
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
	color_t bgcolor;
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
};

enum html_special_type {
	SP_TAG,
	SP_CONTROL,
	SP_TABLE,
	SP_USED,
	SP_FRAMESET,
	SP_FRAME,
	SP_NOWRAP,
	SP_REFRESH,
	SP_COLOR_LINK_LINES,
};

/* Interface for both the renderer and the table handling */

extern struct list_head html_stack;

#define format (((struct html_element *) html_stack.next)->attr)
#define par_format (((struct html_element *) html_stack.next)->parattr)
#define html_top (*(struct html_element *) html_stack.next)

void
parse_html(unsigned char *html, unsigned char *eof,
	   void *f, unsigned char *head);

/* Interface for the renderer */

void
init_html_parser(unsigned char *url, struct document_options *options,
		 unsigned char *start, unsigned char *end,
		 struct string *head, struct string *title,
		 void (*put_chars)(void *, unsigned char *, int),
		 void (*line_break)(void *),
		 void *(*special)(void *, enum html_special_type, ...));

void done_html_parser(void);
struct html_element *init_html_parser_state(enum html_element_type type, int align, int margin, int width);
void done_html_parser_state(struct html_element *element);

/* Interface for the table handling */

int parse_element(unsigned char *, unsigned char *, unsigned char **, int *, unsigned char **, unsigned char **);

unsigned char *get_attr_val(unsigned char *, unsigned char *);
int has_attr(unsigned char *, unsigned char *);
int get_num(unsigned char *, unsigned char *);
int get_width(unsigned char *, unsigned char *, int);
int get_bgcolor(unsigned char *, color_t *);
void set_fragment_identifier(unsigned char *attr_name, unsigned char *attr);
void add_fragment_identifier(void *part, unsigned char *attr);

unsigned char *skip_comment(unsigned char *, unsigned char *);

/* Interface for the viewer */

void do_select_submenu(struct terminal *, struct menu_item *, struct session *);
void free_menu(struct menu_item *);

int get_image_map(unsigned char *, unsigned char *, unsigned char *, unsigned char *a, struct menu_item **, struct memory_list **, unsigned char *, unsigned char *, int, int, int);

/* Lifecycle functions for the tags fastfind cache, if being in use. */

void free_tags_lookup(void);
void init_tags_lookup(void);

#endif
