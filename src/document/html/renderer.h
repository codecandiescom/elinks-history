/* $Id: renderer.h,v 1.73 2005/06/14 13:55:14 witekfl Exp $ */

#ifndef EL__DOCUMENT_HTML_RENDERER_H
#define EL__DOCUMENT_HTML_RENDERER_H

#include "document/document.h"

struct box;
struct cache_entry;
struct string;


void render_html_document(struct cache_entry *cached, struct document *document, struct string *buffer);

/* Interface with tables.c */

struct part {
	struct document *document;

	unsigned char *spaces;
	int spaces_len;

	struct box box;

	int max_width;
	int xa;
	int cx, cy;
	int link_num;
};

void expand_lines(struct part *part, int x, int y, int lines, color_T bgcolor);
void check_html_form_hierarchy(struct part *part);

void draw_frame_hchars(struct part *, int, int, int, unsigned char data, color_T bgcolor, color_T fgcolor);
void draw_frame_vchars(struct part *, int, int, int, unsigned char data, color_T bgcolor, color_T fgcolor);

void free_table_cache(void);

struct part *format_html_part(unsigned char *, unsigned char *, int, int, int, struct document *, int, int, unsigned char *, int);

int find_tag(struct document *document, unsigned char *name, int namelen);

#endif
