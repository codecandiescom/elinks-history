/* Implementation of a login manager for HTML forms */
/* $Id: formhist.c,v 1.6 2003/08/02 15:51:03 jonas Exp $ */

/* TODO: Remember multiple login for the same form
 * TODO: Password manager GUI (here?) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef FORMS_MEMORY

#include <stdio.h>

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

INIT_LIST_HEAD(saved_forms);

static int loaded = 0;

static void
done_form_history_item(struct form_history_item *form)
{
	if (form->url) mem_free(form->url);

	while (!list_empty(form->submit)) {
		struct submitted_value *sv = form->submit.next;

		del_from_list(sv);
		if (sv->name) mem_free(sv->name);
		if (sv->value) mem_free(sv->value);
		mem_free(sv);
	}

        mem_free(form);
}

static int
load_saved_forms(void)
{
	struct form_history_item *form;
	struct submitted_value *sv;
	unsigned char name[MAX_STR_LEN], value[MAX_STR_LEN];
	unsigned char tmp[MAX_STR_LEN], *file;
	FILE *f;

	file = straconcat(elinks_home, "password", NULL);
	if (!file) return 0;
	
	f = fopen(file, "a+");
	mem_free(file);
	if (!f) return 0;

	while (safe_fgets(tmp, MAX_STR_LEN, f)) {
		if (tmp[0] == '\n' && !tmp[1]) continue;

		tmp[strlen(tmp) - 1] = '\0';

		form = mem_alloc(sizeof(struct form_history_item));
		if (!form) return 0;

		init_list(*form);
		init_list(form->submit);
		form->url = stracpy(tmp);
		if (!form->url) goto fail;

		while (safe_fgets(tmp, MAX_STR_LEN, f)) {
			if (tmp[0] == '\n' && !tmp[1]) break;

			/* FIXME: i don't like sscanf()... --Zas */
			if (sscanf(tmp, "%s\t%s%*[\n]", name, value) != 2)
				goto fail;

			sv = mem_alloc(sizeof(struct submitted_value));
			if (!sv) goto fail;

			sv->name = stracpy(name);
			if (!sv->name) {
				mem_free(sv);
				goto fail;
			}

			sv->value = base64_decode(value);
			if (!sv->value) {
				mem_free(sv->name);
				mem_free(sv);
				goto fail;
			}

			add_to_list_bottom(form->submit, sv);
		}

		add_to_list_bottom(saved_forms, form);
	}

	fclose(f);
	loaded = 1;

	return 1;

fail:
	done_form_history_item(form);
	return 0;
}

/*
 * @url is the URL of the site
 * @name is the name of the form control
 * returns the saved value if present
 *	   or NULL */
unsigned char *
get_saved_control_value(unsigned char *url, unsigned char *name)
{
	struct form_history_item *form;
	struct submitted_value *sv;

	if (!loaded && !load_saved_forms()) return NULL;

	foreach (form, saved_forms) {
		if (strcmp(form->url, url)) continue;

		foreach (sv, form->submit)
			if (!strcmp(sv->name, name))
				return sv->value;
	}

	return NULL;
}

/* @url is the URL of the site
 * @submit is the list of submitted_values
 * returns 1 if the form is already saved in
 *	   0 if not */
static int
form_already_saved(unsigned char *url, struct list_head *submit)
{
	struct form_history_item *form;
	struct submitted_value *sv, *savedsv;

	if (!loaded && !load_saved_forms()) return 0;

	foreach (form, saved_forms) {
		if (strcmp(form->url, url)) continue;

		savedsv = (struct submitted_value *) form->submit.next;
		foreachback (sv, *submit) {
			if (sv->type != FC_TEXT && sv->type != FC_PASSWORD)
				continue;

			if (savedsv == (struct submitted_value *) form->submit.next)
				break;

			if (strcmp(sv->name, savedsv->name) ||
			    strcmp(sv->value, savedsv->value)) return 0;

			savedsv  = savedsv->next;
		}

		if ((sv == (struct submitted_value *) submit)
		     && (savedsv == (struct submitted_value *) form->submit.next)) {
			return 1;
		}
	}
	return 0;
}

/* Appends form data to the password file
 * (form data is url+submitted_value(s))
 * returns 1 on success
 *         0 on failure */
static int
remember_form(struct form_history_item *fmem_data)
{
	struct form_history_item *form, *tmpform;
	struct submitted_value *sv;
	struct secure_save_info *ssi;
	unsigned char *file;

	form = mem_calloc(1, sizeof(struct form_history_item));
	if (!form) return 0;

	init_list(*form);
	init_list(form->submit);
	form->url = stracpy(fmem_data->url);
	if (!form->url) goto fail;

	/* FIXME: i don't like "password" as name for this file. --Zas */
	file = straconcat(elinks_home, "password", NULL);
	if (!file) goto fail;

	ssi = secure_open(file, 0177);
	mem_free(file);
	if (!ssi) goto fail;

	/* We're going to save just <INPUT TYPE="text"> and
	 * <INPUT TYPE="password"> */
	foreach (sv, fmem_data->submit)
		if ((sv->type == FC_TEXT) || (sv->type == FC_PASSWORD)) {
			struct submitted_value *sv2;

			sv2 = mem_alloc(sizeof(struct submitted_value));
			if (!sv2) goto fail;

			sv2->value = stracpy(sv->value);
			if (!sv2->value) {
				mem_free(sv2);
				goto fail;
			}

			sv2->name = stracpy(sv->name);
			if (!sv2->name) {
				mem_free(sv2->name);
				mem_free(sv2);
				goto fail;
			}

			add_to_list_bottom(form->submit, sv2);
		}

	add_to_list(saved_forms, form);

	/* Write the list to password file */
	foreach (tmpform, saved_forms) {
		secure_fprintf(ssi, "%s\n", tmpform->url);
		foreachback (sv, tmpform->submit) {
			unsigned char *encvalue = "";

			/* Obfuscate the password. If we do
			 * $ cat ~/.elinks/password we don't want
			 * someone behind our back to read our password */
			if (sv->value) {
				encvalue = base64_encode(sv->value);
				if (!encvalue) return 0;
			}
			secure_fprintf(ssi, "%s\t%s\n", sv->name, encvalue);

			if (*encvalue) mem_free(encvalue);
		}
		secure_fputc(ssi, '\n');
	}

	secure_close(ssi);

	done_form_history_item(fmem_data);
	return 1;

fail:
	done_form_history_item(form);
	return 0;
}

struct list_head *
memorize_form(struct session *ses, struct list_head *submit,
	      struct form_control *frm)
{
	struct form_history_item *fm_data;
	struct list_head *sb;
	struct submitted_value *sv;
	int save = 0;

	foreach (sv, *submit) {
		if (sv->type == FC_PASSWORD && sv->value && *sv->value) {
			save = 1;
			break;
		}
	}

	if (!save || form_already_saved(frm->action, submit)) return NULL;

	fm_data = mem_alloc(sizeof(struct form_history_item));
	if (!fm_data) return NULL;

	init_list(fm_data->submit);

	/* Set up a new list_head, as @submit will be destroyed as soon as
	 * get_form_url() returns */
	sb = &fm_data->submit;
	sb->next = submit->next;
	sb->prev = submit->prev;
	((struct submitted_value *) sb->next)->prev = (struct submitted_value *) sb;
	((struct submitted_value *) sb->prev)->next = (struct submitted_value *) sb;

	fm_data->url = stracpy(frm->action);
	if (!fm_data->url) {
		mem_free(fm_data);
		return NULL;
	}

	msg_box(ses->tab->term, NULL, 0,
		N_("Form memory"), AL_CENTER,
		N_("Should I remember this login?\n\n"
		   "Please note that passwords will be stored "
		   "obscured (i.e. unencrypted) in a file on your disk.\n\n"
		   "If you are using a valuable password answer NO."),
		fm_data, 2,
		N_("Yes"), remember_form, B_ENTER,
		N_("No"), done_form_history_item, NULL);

	return sb;
}

void
done_form_history(void)
{
	struct form_history_item *form;

	foreach(form, saved_forms)
		done_form_history_item(form);
}

#endif /* FORMS_MEMORY */
