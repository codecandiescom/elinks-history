/* $Id: renderer.h,v 1.49 2003/10/30 14:04:57 zas Exp $ */

#ifndef EL__DOCUMENT_HTML_RENDERER_H
#define EL__DOCUMENT_HTML_RENDERER_H

#include "intl/charsets.h"
#include "document/document.h"
#include "document/html/frames.h"
#include "terminal/draw.h"
#include "lowlevel/ttime.h"
#include "util/color.h"
#include "util/lists.h"
/* We need this included later, otherwise it will miss some our
 * declarations. */
/* #include "vs.h" */


struct view_state;
struct document_options;
void cached_format_html(struct view_state *, struct document_view *, struct document_options *);

struct session;
void html_interpret(struct session *);

/* Interface with tables.c */

struct part {
	struct document *document;

	unsigned char *spaces;
	int spaces_len;

	int width, height;
	int xp, yp;
	int xmax;
	int xa;
	int cx, cy;
	int link_num;
};

int expand_line(struct part *, int, int);
int expand_lines(struct part *, int);

void draw_frame_hchars(struct part *, int, int, int, unsigned char data);
void draw_frame_vchars(struct part *, int, int, int, unsigned char data);

void free_table_cache(void);

struct part *format_html_part(unsigned char *, unsigned char *, int, int, int, struct document *, int, int, unsigned char *, int);

/* Interface with parser.c */

/* FIXME: Following probably breaks encapsulation of renderer? --pasky */
extern int margin;

struct conv_table *get_convert_table(unsigned char *head, int to_cp, int default_cp, int *from_cp, enum cp_status *cp_status, int ignore_server_cp);

#endif
