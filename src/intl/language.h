/* $Id: language.h,v 1.10 2003/01/03 01:02:16 pasky Exp $ */

#ifndef EL__LANGUAGE_H
#define EL__LANGUAGE_H

#include "lowlevel/terminal.h"

#include "intl/lang_defs.h"

extern unsigned char dummyarray[];

extern int current_language;

void init_trans();
void shutdown_trans();
unsigned char *get_text_translation(unsigned char *, struct terminal *);
void set_language(int);
int n_languages();
unsigned char *language_name(int);
unsigned char *language_iso639_code(int);

#if 0
#define _(_x_, _y_) get_text_translation(_x_, _y_)
#define N_(x) (dummyarray + x)
#else
#define _(_x_, _y_) (_y_=_y_,_x_)
#define N_(x) (x)
#endif

#endif
