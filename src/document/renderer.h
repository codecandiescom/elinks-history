/* $Id: renderer.h,v 1.4 2004/05/21 10:54:49 jonas Exp $ */

#ifndef EL__DOCUMENT_RENDERER_H
#define EL__DOCUMENT_RENDERER_H

#include "document/document.h"

struct conv_table;
struct document_options;
struct document_view;
struct session;
struct view_state;

void render_document(struct view_state *, struct document_view *, struct document_options *);
void render_document_frames(struct session *ses);
struct conv_table *get_convert_table(unsigned char *head, int to_cp, int default_cp, int *from_cp, enum cp_status *cp_status, int ignore_server_cp);


#endif
