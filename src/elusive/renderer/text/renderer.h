/* $Id: renderer.h,v 1.2 2003/01/01 17:14:40 pasky Exp $ */

#ifndef EL__USIVE_RENDERER_TEXT_RENDERER_H
#define EL__USIVE_RENDERER_TEXT_RENDERER_H

#include "elusive/renderer/renderer.h"

#include "document/options.h"
/* input is struct document_options * */

#include "document/html/renderer.h"
/* output is struct f_data_c * */

/* TODO: Move input and output definitions here! */

extern struct renderer_backend text_renderer_backend;

#endif
