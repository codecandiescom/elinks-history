/* $Id: language.h,v 1.3 2002/07/11 16:14:56 pasky Exp $ */

#ifndef EL__LANGUAGE_H
#define EL__LANGUAGE_H

#include "lowlevel/terminal.h"

#include "intl/lang_defs.h"

extern unsigned char dummyarray[];

extern int current_language;

void init_trans();
void shutdown_trans();
unsigned char *get_text_translation(unsigned char *, struct terminal *);
unsigned char *get_english_translation(unsigned char *);
void set_language(int);
int n_languages();
unsigned char *language_name(int);
unsigned char *language_iso639_code(int);

#define _(_x_, _y_) get_text_translation(_x_, _y_)
#define TEXT(x) (dummyarray + x)

#endif
