#ifndef EL__HTML_R_H
#define EL__HTML_R_H

#include "charsets.h"
#include "links.h" /* list_head */
#include "html.h"
#include "select.h" 
#include "session.h"
/* We need this included later, otherwise view.h will miss some our
 * declarations. */
/* #include "view.h" */

struct tag {
	struct tag *next;
	struct tag *prev;
	int x;
	int y;
	unsigned char name[1];
};

struct node {
	struct node *next;
	struct node *prev;
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
	struct frame_desc f[1];
};

/* For struct f_data */
struct line {
	int l;
	chr c;
	chr *d;
};

/* For struct link */
typedef enum {
	L_LINK,
	L_BUTTON,
	L_CHECKBOX,
	L_SELECT,
	L_FIELD,
	L_AREA,
} link_type;

/* For struct link */
struct point {
	int x;
	int y;
};

/* For struct f_data */
struct link {
	link_type type;
	int num;
	long accesskey;
	unsigned char *where;
	unsigned char *target;
	unsigned char *where_img;
	struct form_control *form;
	unsigned sel_color;
	int n;
	struct point *pos;
};

/* For struct f_data_c */
struct link_bg {
	int x, y;
	unsigned c;
};

/* For struct f_data */
struct document_options {
	int xw, yw; /* size of window */
	int xp, yp; /* pos of window */
	int col, cp, assume_cp, hard_assume;
	int use_document_colours;
	int avoid_dark_on_black;
	int tables, frames, images, margin;  /* if you add anything, fix it in compare_opt */
	int plain;
	int num_links, table_order;
	struct rgb default_fg;
	struct rgb default_bg;
	struct rgb default_link;
	struct rgb default_vlink;
	unsigned char *framename;
};

/* For struct f_data */
struct search {
	unsigned char c;
	int n:24;	/* This structure is size-critical */
	int x, y;
};

struct f_data {
	struct f_data *next;
	struct f_data *prev;
	int refcount;
	unsigned char *url;
	struct document_options opt;
	unsigned char *title;
	int cp, ass;
	int x, y; /* size of document */
	ttime time_to_get;
	tcount use_tag;
	int frame;
	struct frameset_desc *frame_desc;
	int bg;
	struct line *data;
	struct link *links;
	int nlinks;
	struct link **lines1;
	struct link **lines2;
	struct list_head forms;
	struct list_head tags;
	struct list_head nodes;
	struct search *search;
	int nsearch;
	struct search **slines1;
	struct search **slines2;
};

#include "view.h"

struct f_data_c {
	struct f_data_c *next;
	struct f_data_c *prev;
	int used;
	unsigned char *name;
	struct f_data *f_data;
	int xw, yw; /* size of window */
	int xp, yp; /* pos of window */
	int xl, yl; /* last pos of window */
	struct link_bg *link_bg;
	int link_bg_n;
	unsigned char **search_word;
	struct view_state *vs;
	int depth;
};

extern struct document_options *d_opt;

extern int format_cache_entries;

long formatted_info(int);

void shrink_format_cache(int);
void count_format_cache();
void delete_unused_format_cache_entries();
void format_cache_reactivate(struct f_data *);

void cached_format_html(struct view_state *, struct f_data_c *, struct document_options *);
void html_interpret(struct session *);
void get_search_data(struct f_data *);

void destroy_fc(struct form_control *);

/* Interface with html_tbl.c */

struct part {
	int x, y;
	int xp, yp;
	int xmax;
	int xa;
	int cx, cy;
	struct f_data *data;
	int bgcolor;
	unsigned char *spaces;
	int spl;
	int link_num;
	struct list_head uf;
};

int expand_line(struct part *, int, int);
int expand_lines(struct part *, int);
void xset_hchar(struct part *, int, int, unsigned);
void xset_hchars(struct part *, int, int, int, unsigned);

void free_table_cache();

struct part *format_html_part(unsigned char *, unsigned char *, int, int, int, struct f_data *, int, int, unsigned char *, int);

/* Interface with html.c */

/* FIXME: Following probably breaks encapsulation of renderer? --pasky */
extern int margin;

struct conv_table *get_convert_table(unsigned char *, int, int, int *, int *, int);

#endif
