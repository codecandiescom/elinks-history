/* Implementation of a login manager for HTML forms */
/* $Id: formhist.c,v 1.20 2003/08/02 20:05:48 jonas Exp $ */

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


#define FORM_HISTORY_FILENAME "formhist"

static INIT_LIST_HEAD(form_history);

static int form_history_dirty;
static int loaded = 0;

static void
done_form_history_item(struct form_history_item *item)
{
	while (!list_empty(item->submit)) {
		struct submitted_value *sv = item->submit.next;

		del_from_list(sv);
		if (sv->name) mem_free(sv->name);
		if (sv->value) mem_free(sv->value);
		mem_free(sv);
	}

        mem_free(item);
}

static struct form_history_item *
init_form_history_item(unsigned char *url)
{
	struct form_history_item *item;
	int urllen = strlen(url) + 1;

	item = mem_calloc(1, sizeof(struct form_history_item) + urllen);
	if (!item) return NULL;

	init_list(item->submit);
	memcpy(item->url, url, urllen);

	return item;
}

static void
write_form_history(void)
{
	struct form_history_item *item;
	struct secure_save_info *ssi;
	struct string filename;
 
 	if (!form_history_dirty
	    || !elinks_home
	    || !init_string(&filename))
		return;
 
	if (!add_to_string(&filename, elinks_home)
	    || !add_to_string(&filename, FORM_HISTORY_FILENAME)) {
		done_string(&filename);
		return;
	}
 
	ssi = secure_open(filename.source, 0177);
	done_string(&filename);
	if (!ssi) return;

	/* Write the list to formhist file */
	foreach (item, form_history) {
		struct submitted_value *sv;

		secure_fprintf(ssi, "%s\n", item->url);
		foreachback (sv, item->submit) {
			unsigned char *encvalue = "";

			/* Obfuscate the password. If we do
			 * $ cat ~/.elinks/password we don't want
			 * someone behind our back to read our password */
			if (sv->value) {
				encvalue = base64_encode(sv->value);
				if (!encvalue) continue;
			}

			secure_fprintf(ssi, "%s\t%s\n", sv->name, encvalue);

			if (*encvalue) mem_free(encvalue);
		}

		secure_fputc(ssi, '\n');
	}

	secure_close(ssi);
}

void
done_form_history(void)
{
	struct form_history_item *item;

	write_form_history();

	foreach(item, form_history)
		done_form_history_item(item);
}

static struct form_history_item *
read_item_submit_list(struct form_history_item *item, FILE *f)
{
	unsigned char name[MAX_STR_LEN], value[MAX_STR_LEN];
	unsigned char tmp[MAX_STR_LEN];

	while (safe_fgets(tmp, MAX_STR_LEN, f)) {
		struct submitted_value *sv;

		if (tmp[0] == '\n' && !tmp[1]) break;

		/* FIXME: i don't like sscanf()... --Zas */
		if (sscanf(tmp, "%s\t%s%*[\n]", name, value) != 2)
			return NULL;

		sv = mem_alloc(sizeof(struct submitted_value));
		if (!sv) return NULL;

		sv->name = stracpy(name);
		if (!sv->name) {
			mem_free(sv);
			return NULL;
		}

		sv->value = base64_decode(value);
		if (!sv->value) {
			mem_free(sv->name);
			mem_free(sv);
			return NULL;
		}

		add_to_list_bottom(item->submit, sv);
	}

	return item;
}

static int
init_form_history(void)
{
	struct form_history_item *form;
	unsigned char tmp[MAX_STR_LEN], *filename = FORM_HISTORY_FILENAME;
	FILE *f;
	int ret = 1;

	if (elinks_home) {
		filename = straconcat(elinks_home, filename, NULL);
		if (!filename) return 0;
	}

	f = fopen(filename, "r");
	if (elinks_home) mem_free(filename);
	if (!f) return 0;

	while (safe_fgets(tmp, MAX_STR_LEN, f)) {
		if (tmp[0] == '\n' && !tmp[1]) continue;

		tmp[strlen(tmp) - 1] = '\0';

		form = init_form_history_item(tmp);
		if (!form) return 0;

		if (!read_item_submit_list(form, f)) {
			done_form_history_item(form);
			ret = 0;
			break;
		}

		add_to_list_bottom(form_history, form);
	}

	fclose(f);
	loaded = 1;

	return ret;
}

unsigned char *
get_form_history_value(unsigned char *url, unsigned char *name)
{
	struct form_history_item *form;
	struct submitted_value *sv;

	if (!loaded && !init_form_history()) return NULL;

	foreach (form, form_history) {
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

	if (!loaded && !init_form_history()) return 0;

	foreach (form, form_history) {
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

/* Appends form data to the formhist file
 * (form data is url+submitted_value(s))
 * returns 1 on success
 *         0 on failure */
static void
add_form_history_item(struct form_history_item *item)
{
	struct submitted_value *sv;

	/* We're going to save just <INPUT TYPE="text"> and
	 * <INPUT TYPE="password"> so purge anything else from @item->submit. */
	foreach (sv, item->submit) {
		if (sv->type != FC_TEXT || sv->type != FC_PASSWORD) {
			struct submitted_value *garbage = sv;

			sv = sv->prev;
			del_from_list(garbage);
			if (garbage->name) mem_free(garbage->name);
			if (garbage->value) mem_free(garbage->value);
			mem_free(garbage);
		}
	}

	add_to_list(form_history, item);
	form_history_dirty = 1;
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

	fm_data = init_form_history_item(frm->action);
	if (!fm_data) return NULL;

	/* Set up a new list_head, as @submit will be destroyed as soon as
	 * get_form_url() returns */
	sb = &fm_data->submit;
	sb->next = submit->next;
	sb->prev = submit->prev;
	((struct submitted_value *) sb->next)->prev = (struct submitted_value *) sb;
	((struct submitted_value *) sb->prev)->next = (struct submitted_value *) sb;

	msg_box(ses->tab->term, NULL, 0,
		N_("Form history"), AL_CENTER,
		N_("Should I remember this login?\n\n"
		   "Please note that passwords will be stored "
		   "obscured (i.e. unencrypted) in a file on your disk.\n\n"
		   "If you are using a valuable password answer NO."),
		fm_data, 2,
		N_("Yes"), add_form_history_item, B_ENTER,
		N_("No"), done_form_history_item, NULL);

	return sb;
}

#endif /* FORMS_MEMORY */
