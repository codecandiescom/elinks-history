/* HTTP Authentication support */
/* $Id: auth.c,v 1.37 2003/07/11 06:06:02 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "dialogs/auth.h"
#include "intl/gettext/libintl.h"
#include "protocol/http/auth.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "protocol/url.h"
#include "sched/session.h"
#include "util/base64.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


INIT_LIST_HEAD(http_auth_basic_list);


/* Find if url/realm is in auth list. If a matching url is found, but realm is
 * NULL, it returns the first record found. If realm isn't NULL, it returns
 * the first record that matches exactly (url and realm) if any. */
static struct http_auth_basic *
find_auth_entry(unsigned char *url, unsigned char *realm)
{
	struct http_auth_basic *match = NULL, *entry;

	if (!url || !*url) return NULL;

	foreach (entry, http_auth_basic_list) {
		if (!strcasecmp(entry->url, url)) {
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
	}

	return match;
}

#define min(x, y) ((x) < (y) ? (x) : (y))

static struct http_auth_basic *
init_auth_entry(unsigned char *auth_url, unsigned char *realm, struct uri *uri)
{
	struct http_auth_basic *entry;

	entry = mem_calloc(1, sizeof(struct http_auth_basic));
	if (!entry) return NULL;

	entry->url = auth_url;

	if (realm) {
		/* Copy realm value. */
		entry->realm = stracpy(realm);
		if (!entry->realm) {
			mem_free(entry);
			return NULL;
		}
	}

	/* Copy user and pass info passed url if any else NULL terminate. */

	entry->uid = mem_alloc(MAX_UID_LEN);
	if (!entry->uid) {
		if (entry->realm) mem_free(entry->realm);
		mem_free(entry);
		return NULL;
	}
	safe_strncpy(entry->uid, uri->user,
		     min(uri->userlen + 1, MAX_UID_LEN));

	entry->passwd = mem_alloc(MAX_PASSWD_LEN);
	if (!entry->passwd) {
		if (entry->realm) mem_free(entry->realm);
		if (entry->uid) mem_free(entry->uid);
		mem_free(entry);
		return NULL;
	}
	safe_strncpy(entry->passwd, uri->password,
		     min(uri->passwordlen + 1, MAX_UID_LEN));

	return entry;
}

/* Add a Basic Auth entry if needed. */
/* Returns:
 *	ADD_AUTH_NONE	if entry do not exists and user/pass are in url
 *	ADD_AUTH_EXIST	if exact entry already exists or is in blocked state
 *	ADD_AUTH_NEW	if entry was added
 *	ADD_AUIH_ERROR	on error. */
enum add_auth_code
add_auth_entry(struct uri *uri, unsigned char *realm)
{
	struct http_auth_basic *entry;
	unsigned char *newurl = get_uri_string(uri);

	if (!newurl) return ADD_AUTH_ERROR;

	/* Is host/realm already known ? */
	entry = find_auth_entry(newurl, realm);
	if (entry) {
		mem_free(newurl);

		assert(entry->valid && entry->uid && entry->passwd);
		if_assert_failed { return ADD_AUTH_ERROR; }

		/* Found an entry. */
		if (entry->blocked == 1) {
			/* Waiting for user/pass in dialog. */
			return ADD_AUTH_EXIST;
		}

		/* In order to use an existing entry it has to match exactly.
		 * This is done step by step. If something isn't equal the
		 * entry is updated and temporarily marked as invalid. When
		 * we first has reached */

		/* If only one realm is defined or they don't compare. */
		if ((!!realm ^ !!entry->realm)
		    || (realm && entry->realm && strcmp(realm, entry->realm))) {
			entry->valid = 0;
			if (entry->realm) mem_free(entry->realm);
			if (realm) {
				entry->realm = stracpy(realm);
				if (!entry->realm) {
					del_auth_entry(entry);
					return ADD_AUTH_ERROR;
				}
			} else {
				entry->realm = NULL;
			}
		}

		if (!entry->valid || strlen(entry->uid) != uri->userlen
		    || strncmp(entry->uid, uri->user, uri->userlen)) {
			entry->valid = 0;
			safe_strncpy(entry->uid, uri->user,
				     min(uri->userlen + 1, MAX_UID_LEN));
		}

		if (!entry->valid || strlen(entry->passwd) != uri->passwordlen
		    || strncmp(entry->passwd, uri->password, uri->passwordlen)) {
			entry->valid = 0;
			safe_strncpy(entry->passwd, uri->password,
				     min(uri->passwordlen + 1, MAX_UID_LEN));
		}

		if (entry->valid) return ADD_AUTH_EXIST;

	} else {
		/* Create a new entry. */
		entry = init_auth_entry(newurl, realm, uri);
		if (!entry) {
			mem_free(newurl);
			return ADD_AUTH_ERROR;
		}

		add_to_list(http_auth_basic_list, entry);
	}

	entry->valid = (*entry->uid && *entry->passwd);

	/* Return whether entry was added with user/pass from url. */
	return (entry->valid ? ADD_AUTH_NONE : ADD_AUTH_NEW);
}

#undef min

/* Find an entry in auth list by url. If url contains user/pass information
 * and entry does not exist then entry is created.
 * If entry exists but user/pass passed in url is different, then entry is
 * updated (but not if user/pass is set in dialog).
 * It returns NULL on failure, or a base 64 encoded user + pass suitable to
 * use in Authorization header. */
unsigned char *
find_auth(struct uri *uri)
{
	struct http_auth_basic *entry = NULL;
	unsigned char *uid, *ret = NULL;
	unsigned char *newurl = get_uri_string(uri);
	unsigned char *user = memacpy(uri->user, uri->userlen);
	unsigned char *pass = memacpy(uri->password, uri->passwordlen);

	if (!newurl || !user || !pass) goto end;

again:
	entry = find_auth_entry(newurl, NULL);

	/* Check is user/pass info is in url. */
	if ((user && *user) || (pass && *pass)) {
		/* If we've got an entry, but with different user/pass or no
		 * entry, then we try to create or modify it and retry. */
		if ((entry && !entry->valid && entry->uid && entry->passwd
		    && (strcmp(user, entry->uid) || strcmp(pass, entry->passwd)))
		   || !entry) {
			if (add_auth_entry(uri, NULL) == ADD_AUTH_NONE) {
				/* An entry was re-created, we free user/pass
				 * before retry to prevent infinite loop. */
				if (user) {
					mem_free(user);
					user = NULL;
				}
				if (pass) {
					mem_free(pass);
					pass = NULL;
				}
				goto again;
			}
		}
	}

	/* No entry found. */
	if (!entry) goto end;

	/* Sanity check. */
	if (!entry->passwd || !entry->uid) {
		del_auth_entry(entry);
		goto end;
	}

	/* RFC2617 section 2 [Basic Authentication Scheme]
	 * To receive authorization, the client sends the userid and password,
	 * separated by a single colon (":") character, within a base64 [7]
	 * encoded string in the credentials. */

	/* Create base64 encoded string. */
	uid = straconcat(entry->uid, ":", entry->passwd, NULL);
	if (!uid) goto end;

	ret = base64_encode(uid);
	mem_free(uid);

end:
	if (newurl) mem_free(newurl);
	if (user) mem_free(user);
	if (pass) mem_free(pass);

	return ret;
}

/* Delete an entry from auth list. */
void
del_auth_entry(struct http_auth_basic *entry)
{
	if (entry->url) mem_free(entry->url);
	if (entry->realm) mem_free(entry->realm);
	if (entry->uid) mem_free(entry->uid);
	if (entry->passwd) mem_free(entry->passwd);
	del_from_list(entry);
	mem_free(entry);
}

/* Free all entries in auth list and questions in queue. */
void
free_auth(void)
{
	while (!list_empty(http_auth_basic_list))
		del_auth_entry(http_auth_basic_list.next);

	free_list(questions_queue);
}
