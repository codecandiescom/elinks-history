/* Support for multiple languages */
/* $Id: language.c,v 1.12 2002/11/29 18:39:07 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "links.h"

#include "lowlevel/terminal.h"
#include "intl/charsets.h"
#include "intl/language.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

struct translation {
	int code;
	unsigned char *name;
};

struct translation_desc {
	struct translation *t;
};

unsigned char dummyarray[T__N_TEXTS];

#include "language.inc"

unsigned char **translation_array[N_LANGUAGES][N_CODEPAGES];

int current_language;
int current_lang_charset;


void
init_trans()
{
	int i, j;

	for (i = 0; i < N_LANGUAGES; i++)
		for (j = 0; j < N_CODEPAGES; j++)
			translation_array[i][j] = NULL;

	current_language = 0;
	current_lang_charset = 0;
}

void
shutdown_trans()
{
	int i, j, k;

	for (i = 0; i < N_LANGUAGES; i++)
		for (j = 0; j < N_CODEPAGES; j++)
			if (translation_array[i][j]) {
				for (k = 0; k < T__N_TEXTS; k++)
					if (translation_array[i][j][k])
						mem_free(translation_array[i][j][k]);

				mem_free(translation_array[i][j]);
			}
}

unsigned char *
get_text_translation(unsigned char *text, struct terminal *term)
{
	struct list_head *opt_tree = (struct list_head *) term->spec->ptr;
	unsigned char **current_tra;
	struct conv_table *conv_table;
	unsigned char *trn;
	int charset = get_opt_int_tree(opt_tree, "charset");

	if (text < dummyarray || text > dummyarray + T__N_TEXTS)
		return text;

	current_tra = translation_array[current_language][charset];
	if (current_tra) {
		unsigned char *tt;

		trn = current_tra[text - dummyarray];
		if (trn) return trn;

tr:
		tt = translations[current_language].t[text - dummyarray].name;
		if (!tt) {
			trn = stracpy(translation_english[text - dummyarray].name);
		} else {
			conv_table = get_translation_table(current_lang_charset,
							   charset);
			trn = convert_string(conv_table, tt, strlen(tt));
		}
		current_tra[text - dummyarray] = trn;

	} else {
		if (current_lang_charset
		    && charset != current_lang_charset) {
			current_tra = mem_calloc(T__N_TEXTS, sizeof (unsigned char **));
			translation_array[current_language][charset] = current_tra;

			if (current_tra)
				goto tr;
		}

		trn = translations[current_language].t[text - dummyarray].name;
		if (!trn) {
			trn = translation_english[text - dummyarray].name;
			translations[current_language].t[text - dummyarray].name = trn;	/* modifying translation structure */
		}
	}

	return trn;
}

unsigned char *
get_english_translation(unsigned char *text)
{
	if (text < dummyarray || text > dummyarray + T__N_TEXTS)
		return text;

	return translation_english[text - dummyarray].name;
}

int
n_languages()
{
	return N_LANGUAGES;
}

unsigned char *
language_name(int l)
{
	return translations[l].t[T__LANGUAGE].name;
}

unsigned char *
language_iso639_code(int l)
{
	/* XXX: In fact this is _NOT_ a real ISO639 code but RFC3066 code (as
	 * we're supposed to use that one when sending language tags through
	 * HTTP/1.1) and that one consists basically from ISO639[-ISO3166].
	 * This is important for ie. pt vs pt-BR. */
	/* TODO: We should reflect this in name of this function and of the
	 * tag. --pasky */
	return translations[l].t[T__ISO_639_CODE].name;
}

void
set_language(int l)
{
	int i;
	unsigned char *cp;

	for (i = 0; i < T__N_TEXTS; i++)
		if (translations[l].t[i].code != i) {
			internal("Bad table for language %s. Run script synclang.",
				 translations[l].t[T__LANGUAGE].name);
			return;
		}

	current_language = l;
	cp = translations[l].t[T__CHAR_SET].name;
	i = get_cp_index(cp);

	if (i == -1) {
		internal("Unknown charset for language %s.",
			 translations[l].t[T__LANGUAGE].name);
		i = 0;
	}
	current_lang_charset = i;
}
