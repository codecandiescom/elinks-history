/* Implementation of a login manager for HTML forms */
/* $Id: formsmem.c,v 1.2 2003/08/01 18:14:49 jonas Exp $ */

/* TODO: Remember multiple login for the same form
 * TODO: Password manager GUI (here?) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef FORMS_MEMORY

#include <stdio.h>

#include "elinks.h"

#include "document/html/parser.h"
#include "viewer/text/form.h"
#include "formsmem/formsmem.h"
#include "lowlevel/home.h"
#include "util/base64.h"
#include "util/file.h"
#include "util/lists.h"
#include "util/secsave.h"
#include "util/string.h"

INIT_LIST_HEAD(saved_forms);

static int loaded = 0;

static int
load_saved_forms(void)
{
	struct formsmem_data *form;
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

		form = mem_alloc(sizeof(struct formsmem_data));
		if (!form) return 0;

		form->submit = mem_alloc(sizeof(struct list_head));
		if (!form->submit) {
			mem_free(form);
			return 0;
		}

		init_list(*form);
		init_list(*form->submit);
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

			add_to_list_bottom(*form->submit, sv);
		}

		add_to_list_bottom(saved_forms, form);
	}

	fclose(f);
	loaded = 1;

	return 1;

fail:
	free_form(form);
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
	struct formsmem_data *form;
	struct submitted_value *sv;

	if (!loaded && !load_saved_forms()) return NULL;

	foreach (form, saved_forms) {
		if (strcmp(form->url, url)) continue;

		foreach (sv, *form->submit)
			if (!strcmp(sv->name, name))
				return sv->value;
	}

	return NULL;
}

/* @url is the URL of the site
 * @submit is the list of submitted_values
 * returns 1 if the form is already saved in
 *	   0 if not */
int
form_already_saved(unsigned char *url, struct list_head *submit)
{
	struct formsmem_data *form;
	struct submitted_value *sv, *savedsv;

	if (!loaded && !load_saved_forms()) return 0;

	foreach (form, saved_forms) {
		if (strcmp(form->url, url)) continue;

		savedsv = (struct submitted_value *) form->submit->next;
		foreachback (sv, *submit) {
			if (sv->type != FC_TEXT && sv->type != FC_PASSWORD)
				continue;

			if (savedsv == (struct submitted_value *) form->submit)
				break;

			if (strcmp(sv->name, savedsv->name) ||
			    strcmp(sv->value, savedsv->value)) return 0;

			savedsv  = savedsv->next;
		}

		if ((sv == (struct submitted_value *) submit)
		     && (savedsv == (struct submitted_value *) form->submit)) {
			return 1;
		}
	}
	return 0;
}

/* Appends form data to the password file
 * (form data is url+submitted_value(s))
 * returns 1 on success
 *         0 on failure */
int
remember_form(struct formsmem_data *fmem_data)
{
	struct formsmem_data *form, *tmpform;
	struct submitted_value *sv;
	struct secure_save_info *ssi;
	unsigned char *file;

	form = mem_calloc(1, sizeof(struct formsmem_data));
	if (!form) return 0;

	form->submit = mem_alloc(sizeof(struct list_head));
	if (!form->submit) {
		mem_free(form);
		return 0;
	}

	init_list(*form);
	init_list(*form->submit);
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
	foreach (sv, *fmem_data->submit)
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

			add_to_list_bottom(*form->submit, sv2);
		}

	add_to_list(saved_forms, form);

	/* Write the list to password file */
	foreach (tmpform, saved_forms) {
		secure_fprintf(ssi, "%s\n", tmpform->url);
		foreachback (sv, *tmpform->submit) {
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

	free_form(fmem_data);
	return 1;

fail:
	free_form(form);
	return 0;
}

void
free_formsmemory(void)
{
	struct formsmem_data *form;

	foreach(form, saved_forms)
		free_form(form);
}

void
free_form(struct formsmem_data *form)
{
	struct submitted_value *sv;

	if (form->url) mem_free(form->url);

	foreachback (sv, *form->submit) {
		if (sv->name) mem_free(sv->name);
		if (sv->value) mem_free(sv->value);
		mem_free(sv);
	}

        if (form->submit) mem_free(form->submit);
        mem_free(form);
}

#endif /* FORMS_MEMORY */
