/* HTTP Authentication support */
/* $Id: auth.c,v 1.86 2004/06/13 13:57:06 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/hierbox.h"
#include "intl/gettext/libintl.h"
#include "protocol/auth/auth.h"
#include "protocol/auth/dialogs.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "sched/session.h"
#include "util/base64.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

/* Defines to 1 to enable http auth debugging output. */
#if 0
#define DEBUG_HTTP_AUTH
#endif

static INIT_LIST_HEAD(http_auth_basic_list);


/* Find if url/realm is in auth list. If a matching url is found, but realm is
 * NULL, it returns the first record found. If realm isn't NULL, it returns
 * the first record that matches exactly (url and realm) if any. */
static struct http_auth_basic *
find_auth_entry(struct uri *uri, unsigned char *realm)
{
	struct http_auth_basic *match = NULL, *entry;

#ifdef DEBUG_HTTP_AUTH
	DBG("find_auth_entry: url=%s realm=%s", url, realm);
#endif

	foreach (entry, http_auth_basic_list) {
		if (!compare_uri(entry->uri, uri, URI_HTTP_AUTH))
			continue;

		/* Found a matching url. */
		match = entry;
		if (!realm) {
			/* Since realm is NULL, stops immediatly. */
			break;
		}

		/* From RFC 2617 section 1.2:
		 * The realm value (case-sensitive), in combination
		 * with the canonical root URL (the absolute URI for
		 * the server whose abs_path is empty; see section
		 * 5.1.2 of [2]) of the server being accessed, defines
		 * the protection space. */
		if (entry->realm && !strcmp(entry->realm, realm)) {
			/* Exact match. */
			break; /* Stop here. */
		}
	}

	return match;
}

#define set_auth_user(e, u) \
	do { \
		int userlen = int_min((u)->userlen, HTTP_AUTH_USER_MAXLEN - 1); \
		if (userlen) \
			memcpy((e)->user, (u)->user, userlen); \
		(e)->user[userlen] = 0; \
	} while (0)

#define set_auth_password(e, u) \
	do { \
		int passwordlen = int_min((u)->passwordlen, HTTP_AUTH_PASSWORD_MAXLEN - 1); \
		if (passwordlen) \
			memcpy((e)->password, (u)->password, passwordlen); \
		(e)->password[passwordlen] = 0; \
	} while (0)

static struct http_auth_basic *
init_auth_entry(struct uri *uri, unsigned char *realm)
{
	struct http_auth_basic *entry;

#ifdef DEBUG_HTTP_AUTH
	DBG("init_auth_entry: auth_url=%s realm=%s uri=%p", auth_url, realm, uri);
#endif

	entry = mem_calloc(1, sizeof(struct http_auth_basic));
	if (!entry) return NULL;

	entry->uri = get_uri_reference(uri);

	if (realm) {
		/* Copy realm value. */
		entry->realm = stracpy(realm);
		if (!entry->realm) {
			mem_free(entry);
			return NULL;
		}
	}

	/* Copy user and pass info passed url if any else NULL terminate. */

	set_auth_user(entry, uri);
	set_auth_password(entry, uri);

	entry->box_item = add_listbox_leaf(&auth_browser, NULL, entry);

	return entry;
}

/* Add a Basic Auth entry if needed. */
/* Returns the new entry or updates an existing one. Sets the @valid member if
 * updating is required so it can be tested if the user should be queried. */
struct http_auth_basic *
add_auth_entry(struct uri *uri, unsigned char *realm)
{
	struct http_auth_basic *entry;

#ifdef DEBUG_HTTP_AUTH
	DBG("add_auth_entry: newurl=%s realm=%s uri=%p", newurl, realm, uri);
#endif

	/* Is host/realm already known ? */
	entry = find_auth_entry(uri, realm);
	if (entry) {
		/* Waiting for user/pass in dialog. */
		if (entry->blocked) return NULL;

		/* In order to use an existing entry it has to match exactly.
		 * This is done step by step. If something isn't equal the
		 * entry is updated and marked as invalid. */

		/* If only one realm is defined or they don't compare. */
		if ((!!realm ^ !!entry->realm)
		    || (realm && entry->realm && strcmp(realm, entry->realm))) {
			entry->valid = 0;
			mem_free_set(&entry->realm, NULL);
			if (realm) {
				entry->realm = stracpy(realm);
				if (!entry->realm) {
					del_auth_entry(entry);
					return NULL;
				}
			}
		}

		if (!*entry->user
		    || (!uri->user || !uri->userlen ||
			strlcmp(entry->user, -1, uri->user, uri->userlen))) {
			entry->valid = 0;
			set_auth_user(entry, uri);
		}

		if (!*entry->password
		    || (!uri->password || !uri->passwordlen ||
			strlcmp(entry->password, -1, uri->password, uri->passwordlen))) {
			entry->valid = 0;
			set_auth_password(entry, uri);
		}

	} else {
		/* Create a new entry. */
		entry = init_auth_entry(uri, realm);
		if (!entry) return NULL;

		add_to_list(http_auth_basic_list, entry);
	}

	/* Only pop up question if one of the protocols requested it */
	if (entry && !entry->valid && entry->realm)
		add_questions_entry(do_auth_dialog, entry);

	return entry;
}

/* Find an entry in auth list by url. If url contains user/pass information
 * and entry does not exist then entry is created.
 * If entry exists but user/pass passed in url is different, then entry is
 * updated (but not if user/pass is set in dialog). */
/* It returns a base 64 encoded user + pass suitable to use in Authorization
 * header, or NULL on failure. */
unsigned char *
find_auth(struct uri *uri)
{
	struct http_auth_basic *entry = NULL;
	unsigned char *id, *ret;

#ifdef DEBUG_HTTP_AUTH
	DBG("find_auth: newurl=%s uri=%p", newurl, uri);
#endif

	entry = find_auth_entry(uri, NULL);

	/* Check is user/pass info is in url. */
	if (uri->userlen || uri->passwordlen) {
		/* If there's no entry a new one is added else if the entry
		 * does not correspond to any existing one update it with the
		 * user and password from the uri. */
		if (!entry
		    || (auth_entry_has_userinfo(entry)
		        && !strlcmp(entry->password, -1, uri->password, uri->passwordlen)
		        && !strlcmp(entry->user, -1, uri->user, uri->userlen))) {

			entry = add_auth_entry(uri, NULL);
		}
	}

	/* No entry found or waiting for user/password in dialog. */
	if (!entry || entry->blocked)
		return NULL;

	/* Sanity check. */
	if (!auth_entry_has_userinfo(entry)) {
		del_auth_entry(entry);
		return NULL;
	}

	/* RFC2617 section 2 [Basic Authentication Scheme]
	 * To receive authorization, the client sends the userid and password,
	 * separated by a single colon (":") character, within a base64 [7]
	 * encoded string in the credentials. */

	/* Create base64 encoded string. */
	id = straconcat(entry->user, ":", entry->password, NULL);
	if (!id) return NULL;
	ret = base64_encode(id);
	mem_free(id);

	return ret;
}

/* Delete an entry from auth list. */
void
del_auth_entry(struct http_auth_basic *entry)
{
#ifdef DEBUG_HTTP_AUTH
	DBG("del_auth_entry: url=%s realm=%s user=%p",
	      entry->url, entry->realm, entry->user);
#endif

	if (entry->box_item)
		done_listbox_item(&auth_browser, entry->box_item);
	done_uri(entry->uri);
	mem_free_if(entry->realm);

	del_from_list(entry);
	mem_free(entry);
}

/* Free all entries in auth list and questions in queue. */
void
free_auth(void)
{
#ifdef DEBUG_HTTP_AUTH
	DBG("free_auth");
#endif

	while (!list_empty(http_auth_basic_list))
		del_auth_entry(http_auth_basic_list.next);

	free_list(questions_queue);
}

struct http_auth_basic *
get_invalid_auth_entry(void)
{
	struct http_auth_basic *entry;

#ifdef DEBUG_HTTP_AUTH
	DBG("get_invalid_auth_entry");
#endif

	foreach (entry, http_auth_basic_list)
		if (!entry->valid)
			return entry;

	return NULL;
}
