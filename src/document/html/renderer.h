/* $Id: renderer.h,v 1.33 2003/07/30 16:14:36 jonas Exp $ */

#ifndef EL__DOCUMENT_HTML_RENDERER_H
#define EL__DOCUMENT_HTML_RENDERER_H

#include "intl/charsets.h"
#include "document/options.h"
#include "document/html/parser.h"
#include "terminal/draw.h"
#include "terminal/screen.h"
#include "lowlevel/ttime.h"
#include "sched/session.h"
#include "util/lists.h"
/* We need this included later, otherwise it will miss some our
 * declarations. */
/* #include "vs.h" */


/* XXX: Please try to keep order of fields from max. to min. of size
 * of each type of fields:
 *
 * Prefer:
 *	long a;
 *	int b;
 *	char c;
 * Instead of:
 *	char c;
 *	int b;
 *	long b;
 *
 * It will help to reduce memory padding on some architectures.
 * It's not a perfect solution, but better than worse.
 */

struct tag {
	LIST_HEAD(struct tag);

	int x, y;
	unsigned char name[1]; /* must be last of struct. --Zas */
};

struct node {
	LIST_HEAD(struct node);

	int x, y;
	int xw, yw;
};


/* For struct link */
enum link_type {
	L_LINK,
	L_BUTTON,
	L_CHECKBOX,
	L_SELECT,
	L_FIELD,
	L_AREA,
};

/* For struct link */
struct point {
	int x, y;
};

/* For struct f_data */
struct link {
	long accesskey;

	enum link_type type;

	unsigned char *where;
	unsigned char *target;
	unsigned char *where_img;
	unsigned char *title;
	unsigned char *name;

	struct form_control *form;
	struct point *pos;

	int n;
	int num;

	unsigned sel_color;
};

/* For struct document_view */
struct link_bg {
	int x, y;
	struct screen_char c;
};

/* For struct f_data */
struct search {
	int x, y;
	signed int n:24;	/* This structure is size-critical */
	unsigned char c;
};


extern int format_cache_entries;

#include "document/html/frames.h"


long formatted_info(int);

void shrink_format_cache(int);
void count_format_cache(void);
void delete_unused_format_cache_entries(void);
void format_cache_reactivate(struct document *);

void cached_format_html(struct view_state *, struct document_view *, struct document_options *);
void html_interpret(struct session *);

void destroy_fc(struct form_control *);

/* Interface with html_tbl.c */

struct part {
	struct list_head uf;
	struct document *document;

	unsigned char *spaces;
	int spaces_len;

	int x, y;
	int xp, yp;
	int xmax;
	int xa;
	int cx, cy;
	int link_num;
};

int expand_line(struct part *, int, int);
int expand_lines(struct part *, int);
void xset_hchar(struct part *, int, int, unsigned char, unsigned char);
void xset_hchars(struct part *, int, int, int, unsigned char, unsigned char);

void free_table_cache(void);

struct part *format_html_part(unsigned char *, unsigned char *, int, int, int, struct document *, int, int, unsigned char *, int);

/* Interface with html.c */

/* FIXME: Following probably breaks encapsulation of renderer? --pasky */
extern int margin;

struct conv_table *get_convert_table(unsigned char *head, int to_cp, int default_cp, int *from_cp, enum cp_status *cp_status, int ignore_server_cp);

#endif
