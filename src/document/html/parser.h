/* $Id: parser.h,v 1.35 2003/10/29 16:10:29 jonas Exp $ */

#ifndef EL__DOCUMENT_HTML_PARSER_H
#define EL__DOCUMENT_HTML_PARSER_H

#include "bfu/dialog.h"
#include "bfu/style.h"
#include "document/document.h"
#include "document/html/frames.h"
#include "util/color.h"
#include "util/lists.h"
#include "util/memlist.h"

/* XXX: This is just terible - this interface is from 75% only for other HTML
 * files - there's lack of any well defined interface and it's all randomly
 * mixed up :/. */

struct form {
	unsigned char *action;
	unsigned char *target;
	int method;
	int num;
};

#define NULL_STRUCT_FORM { NULL, NULL, 0, 0 }

extern struct form form;

enum form_method {
	FM_GET,
	FM_POST,
	FM_POST_MP,
};

enum form_type {
	FC_TEXT,
	FC_PASSWORD,
	FC_FILE,
	FC_TEXTAREA,
	FC_CHECKBOX,
	FC_RADIO,
	FC_SELECT,
	FC_SUBMIT,
	FC_IMAGE,
	FC_RESET,
	FC_HIDDEN,
};

struct form_control {
	LIST_HEAD(struct form_control);

	int form_num;
	int ctrl_num;
	int g_ctrl_num;
	int position;
	enum form_method method;
	unsigned char *action;
	unsigned char *target;
	enum form_type type;
	unsigned char *name;
	unsigned char *alt;
	int ro;
	unsigned char *default_value;
	int default_state;
	int size;
	int cols, rows, wrap;
	int maxlength;
	int nvalues;
	unsigned char **values;
	unsigned char **labels;
	struct menu_item *menu;
};

struct form_state {
	int form_num;
	int ctrl_num;
	int g_ctrl_num;
	int position;
	int type;
	unsigned char *value;
	int state;
	int vpos;
	int vypos;
};

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

struct html_element {
	LIST_HEAD(struct html_element);

	struct text_attrib attr;
	struct par_attrib parattr;
	int invisible;
	unsigned char *name;
	int namelen;
	unsigned char *options;
	int linebreak;
	int dontkill;
	struct frameset_desc *frameset;
};

extern struct list_head html_stack;
extern int line_breax;

extern unsigned char *startf;
extern unsigned char *eofff;

#define format (((struct html_element *) html_stack.next)->attr)
#define par_format (((struct html_element *) html_stack.next)->parattr)
#define html_top (*(struct html_element *) html_stack.next)

/* extern void *ff; */
extern void (*put_chars_f)(void *, unsigned char *, int);
extern void (*line_break_f)(void *);
extern void (*init_f)(void *);
extern void *(*special_f)(void *, int, ...);

extern unsigned char *last_form_tag;
extern unsigned char *last_form_attr;
extern unsigned char *last_input_tag;

int parse_element(unsigned char *, unsigned char *, unsigned char **, int *, unsigned char **, unsigned char **);

unsigned char *get_attr_val(unsigned char *, unsigned char *);
int has_attr(unsigned char *, unsigned char *);
int get_num(unsigned char *, unsigned char *);
int get_width(unsigned char *, unsigned char *, int);
/* int get_color(unsigned char *, unsigned char *, color_t *); */
int get_bgcolor(unsigned char *, color_t *);

void html_stack_dup(void);
void kill_html_stack_item(struct html_element *);
unsigned char *skip_comment(unsigned char *, unsigned char *);

struct menu_item;

int get_image_map(unsigned char *, unsigned char *, unsigned char *, unsigned char *a, struct menu_item **, struct memory_list **, unsigned char *, unsigned char *, int, int, int);

void scan_http_equiv(unsigned char *, unsigned char *, struct string *, struct string *);

void
parse_html(unsigned char *html, unsigned char *eof,
	   void (*put_chars)(void *, unsigned char *, int),
	   void (*line_break)(void *),
	   void (*init)(void *),
	   void *(*special)(void *, int, ...),
	   void *f, unsigned char *head);

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


void free_menu(struct menu_item *);

struct session;
void do_select_submenu(struct terminal *, struct menu_item *, struct session *);

/* This releases the tags fastfind cache, if being in use. */
void free_tags_lookup(void);
/* This initializes the tags cache used by fastfind. */
void init_tags_lookup(void);
#endif
