/* $Id: renderer.h,v 1.23 2003/05/08 21:50:07 zas Exp $ */

#ifndef EL__DOCUMENT_HTML_RENDERER_H
#define EL__DOCUMENT_HTML_RENDERER_H

#include "intl/charsets.h"
#include "document/options.h"
#include "document/html/parser.h"
#include "terminal/draw.h"
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

struct frameset_desc;

struct frame_desc {
	struct frameset_desc *subframe;

	unsigned char *name;
	unsigned char *url;

	int line;
	int xw, yw;
};

struct frameset_desc {
	int n;
	int x, y;
	int xp, yp;

	struct frame_desc f[1]; /* must be last of struct. --Zas */
};

/* For struct f_data */
struct line {
	chr *d;

	int l;
	int size;
	int dsize;

	chr c;
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

/* For struct f_data_c */
struct link_bg {
	int x, y;
	unsigned c;
};

/* For struct f_data */
struct search {
	int x, y;
	signed int n:24;	/* This structure is size-critical */
	unsigned char c;
};

struct f_data {
	LIST_HEAD(struct f_data);

	struct document_options opt;

	struct list_head forms;
	struct list_head tags;
	struct list_head nodes;

	unsigned char *url;
	unsigned char *title;

	struct frameset_desc *frame_desc;

	struct line *data;

	struct link *links;
	struct link **lines1;
	struct link **lines2;

	struct search *search;
	struct search **slines1;
	struct search **slines2;

	ttime time_to_get;
	tcount use_tag;

	int refcount;
	int cp, ass;
	int x, y; /* size of document */
	int frame;
	int bg;
	int nlinks;
	int nsearch;
};

#include "viewer/text/vs.h"

struct f_data_c {
	LIST_HEAD(struct f_data_c);

	unsigned char *name;
	unsigned char **search_word;

	struct f_data *f_data;
	struct view_state *vs;
	struct link_bg *link_bg;

	int link_bg_n;
	int xw, yw; /* size of window */
	int xp, yp; /* pos of window */
	int xl, yl; /* last pos of window */
	int depth;
	int used;

};

extern int format_cache_entries;

long formatted_info(int);

void shrink_format_cache(int);
void count_format_cache(void);
void delete_unused_format_cache_entries(void);
void format_cache_reactivate(struct f_data *);

void cached_format_html(struct view_state *, struct f_data_c *, struct document_options *);
void html_interpret(struct session *);
void get_search_data(struct f_data *);

void destroy_fc(struct form_control *);

/* Interface with html_tbl.c */

struct part {
	struct list_head uf;
	unsigned char *spaces;
	struct f_data *data;

	int x, y;
	int xp, yp;
	int xmax;
	int xa;
	int cx, cy;
	int spl;
	int link_num;
};

int expand_line(struct part *, int, int);
int expand_lines(struct part *, int);
void xset_hchar(struct part *, int, int, unsigned);
void xset_hchars(struct part *, int, int, int, unsigned);

void free_table_cache(void);

struct part *format_html_part(unsigned char *, unsigned char *, int, int, int, struct f_data *, int, int, unsigned char *, int);

/* Interface with html.c */

/* FIXME: Following probably breaks encapsulation of renderer? --pasky */
extern int margin;

struct conv_table *get_convert_table(unsigned char *, int, int, int *, int *, int);

#endif
