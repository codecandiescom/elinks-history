/* $Id: renderer.h,v 1.67 2004/06/30 16:10:27 zas Exp $ */

#ifndef EL__DOCUMENT_HTML_RENDERER_H
#define EL__DOCUMENT_HTML_RENDERER_H

#include "document/document.h"

struct box;
struct cache_entry;


void render_html_document(struct cache_entry *cached, struct document *document);

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

void expand_lines(struct part *, int x, int y, int lines);

void draw_frame_hchars(struct part *, int, int, int, unsigned char data, color_t bgcolor, color_t fgcolor);
void draw_frame_vchars(struct part *, int, int, int, unsigned char data, color_t bgcolor, color_t fgcolor);

void free_table_cache(void);

struct part *format_html_part(unsigned char *, unsigned char *, int, int, int, struct document *, int, int, unsigned char *, int);

int find_tag(struct document *document, unsigned char *name, int namelen);

#endif
