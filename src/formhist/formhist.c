/* Implementation of a login manager for HTML forms */
/* $Id: formhist.c,v 1.44 2003/09/05 17:55:43 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef FORMS_MEMORY

#include <string.h>

#include "elinks.h"

#include "bfu/msgbox.h"
#include "document/html/parser.h"
#include "formhist/formhist.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/home.h"
#include "util/base64.h"
#include "util/file.h"
#include "util/lists.h"
#include "util/secsave.h"
#include "util/string.h"
#include "viewer/text/form.h"

/* TODO: Remember multiple login for the same form.
 * TODO: Password manager GUI (here?) (in dialogs.c, of course --pasky). */


#define FORMHIST_FILENAME		"formhist"

INIT_LIST_HEAD(saved_forms);

static int loaded = 0;

static struct submitted_value *
new_submitted_value(unsigned char *name, unsigned char *value)
{
	struct submitted_value *sv;

	sv = mem_alloc(sizeof(struct submitted_value));
	if (!sv) return NULL;

	sv->value = stracpy(value);
	if (!sv->value) { mem_free(sv); return NULL; }

	sv->name = stracpy(name);
	if (!sv->name) { mem_free(sv->value); mem_free(sv); return NULL; }

	return sv;
}


static void inline
free_submitted_value(struct submitted_value *sv)
{
	if (!sv) return;
	if (sv->value) mem_free(sv->value);
	if (sv->name) mem_free(sv->name);
	mem_free(sv);
}

static void
free_form_in_list(struct formhist_data *form)
{
	struct submitted_value *sv, *svtmp;

	foreach (sv, form->submit) {
		svtmp = sv;
		sv = sv->prev;
		del_from_list(svtmp);
		free_submitted_value(svtmp);
	}
}

static struct formhist_data *
new_form(unsigned char *url)
{
	struct formhist_data *form;
	int url_size = strlen(url) + 1;

	form = mem_calloc(1, sizeof(struct formhist_data) + url_size);
	if (!form) return NULL;

	memcpy(form->url, url, url_size);
	init_list(form->submit);

	return form;
}

static void
free_form(struct formhist_data *form)
{
	free_form_in_list(form);
	mem_free(form);
}

static int
load_saved_forms(void)
{
	struct formhist_data *form;
	unsigned char tmp[MAX_STR_LEN];
       	unsigned char *file;
	FILE *f;

	file = straconcat(elinks_home, FORMHIST_FILENAME, NULL);
	if (!file) return 0;

	f = fopen(file, "r");
	mem_free(file);
	if (!f) return 0;

	while (safe_fgets(tmp, MAX_STR_LEN, f)) {
		if (tmp[0] == '\n' && !tmp[1]) continue;

		tmp[strlen(tmp) - 1] = '\0';

		form = new_form(tmp);
		if (!form) continue;

		while (safe_fgets(tmp, MAX_STR_LEN, f)) {
			struct submitted_value *sv;
			unsigned char *name, *value, *p;
			unsigned char *enc_value;

			if (tmp[0] == '\n' && !tmp[1]) break;

			name = tmp;
			p = strchr(name, '\t');
			if (!p) goto fail;
			*p = '\0';

			value = ++p;
			p = strchr(p, '\n');
			if (p) *p = '\0';

			enc_value = base64_decode(value);
			if (!enc_value) goto fail;

			sv = new_submitted_value(name, enc_value);
			mem_free(enc_value);
			if (!sv) goto fail;

			add_to_list(form->submit, sv);
		}
		add_to_list(saved_forms, form);
	}

	fclose(f);
	loaded = 1;

	return 1;

fail:
	free_form(form);
	return 0;
}

static int
save_saved_forms(void)
{
	struct secure_save_info *ssi;
	unsigned char *file;
	struct formhist_data *form;

	file = straconcat(elinks_home, FORMHIST_FILENAME, NULL);
	if (!file) return 0;

	ssi = secure_open(file, 0177);
	mem_free(file);
	if (!ssi) return 0;

	/* Write the list to password file */

	foreach (form, saved_forms) {
		struct submitted_value *sv;

		secure_fprintf(ssi, "%s\n", form->url);

		foreach (sv, form->submit) {
			/* Obfuscate the password. If we do
			 * $ cat ~/.elinks/password
			 * we don't want someone behind our back to read our
			 * password (androids don't count). */
			if (sv->value && *sv->value) {
				unsigned char *encvalue = base64_encode(sv->value);

				if (!encvalue) return 0;
				secure_fprintf(ssi, "%s\t%s\n", sv->name, encvalue);
				mem_free(encvalue);
			} else {
				secure_fprintf(ssi, "%s\t\n", sv->name);
			}
		}

		secure_fputc(ssi, '\n');
	}

	return secure_close(ssi);
}

/* Check whether the form (chain of @submit submitted_values at @url document)
 * is already present in the form history. */
static int
form_already_saved(struct formhist_data *form1)
{
	struct formhist_data *form;

	if (!loaded && !load_saved_forms()) return 0;

	foreach (form, saved_forms) {
		int count = 0;
		int exact = 0;
		struct submitted_value *sv;

		if (strcmp(form->url, form1->url)) continue;

		/* Iterate through submitted entries. */
		foreach (sv, form1->submit) {
			struct submitted_value *sv2;
			unsigned char *value = NULL;

			count++;
			foreach (sv2, form->submit) {
				if (!strcmp(sv->name, sv2->name)) {
					exact++;
					value = sv2->value;
					break;
				}
			}
			/* If we found a value for that name, check if value
			 * has changed or not. */
			if (value && strcmp(sv->value, value)) return 0;
		}

		/* Check if submitted values have changed or not. */
		if (count && exact && count == exact) return 1;
	}

	return 0;
}

static int
forget_forms_with_url(unsigned char *url)
{
	struct formhist_data *form, *tmpform;
	int count = 0;

	foreach (form, saved_forms) {
		if (strcmp(form->url, url)) continue;

		tmpform = form;
		form = form->prev;
		del_from_list(tmpform);
		free_form(tmpform);
		count++;
	}

	return count;
}

/* Appends form data @form1 (url and submitted_value(s)) to the password file.
 * Returns 1 on success, 0 otherwise. */
static int
remember_form(struct formhist_data *form)
{
	forget_forms_with_url(form->url);
	add_to_list(saved_forms, form);

	return save_saved_forms();
}

unsigned char *
get_form_history_value(unsigned char *url, unsigned char *name)
{
	struct formhist_data *form;

	if (!url || !*url || !name || !*name) return NULL;

	if (!loaded && !load_saved_forms()) return NULL;

	foreach (form, saved_forms) {
		if (!strcmp(form->url, url)) {
			struct submitted_value *sv;

			foreach (sv, form->submit)
				if (!strcmp(sv->name, name))
					return sv->value;
		}
	}

	return NULL;
}

void
memorize_form(struct session *ses, struct list_head *submit,
	      struct form_control *frm)
{
	struct formhist_data *form;
	struct submitted_value *sv;
	int save = 0;

	foreach (sv, *submit) {
		if (sv->type == FC_PASSWORD && sv->value && *sv->value) {
			save = 1;
			break;
		}
	}

	if (!save) return;

	form = new_form(frm->action);
	if (!form) return;

	foreach (sv, *submit) {
		if ((sv->type == FC_TEXT) || (sv->type == FC_PASSWORD)) {
			struct submitted_value *sv2;

			sv2 = new_submitted_value(sv->name, sv->value);
			if (!sv2) goto fail;

			add_to_list(form->submit, sv2);
		}
	}

	if (form_already_saved(form)) goto fail;

	msg_box(ses->tab->term, NULL, 0,
		N_("Form memory"), AL_CENTER,
		N_("Should I remember this login?\n\n"
		"Please note that passwords will be stored "
		"obscured (i.e. unencrypted) in a file on your disk.\n\n"
		"If you are using a valuable password answer NO."),
		form, 2,
		N_("Yes"), remember_form, B_ENTER,
		N_("No"), free_form, NULL);

	return;

fail:
	free_form(form);
}

void
done_form_history(void)
{
	struct formhist_data *form;

	foreach(form, saved_forms)
		free_form_in_list(form);

	free_list(saved_forms);
}

#endif /* FORMS_MEMORY */
