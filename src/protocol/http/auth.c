/* HTTP Authentication support */
/* $Id: auth.c,v 1.62 2003/07/23 08:37:47 zas Exp $ */

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
#include "sched/session.h"
#include "util/base64.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


static INIT_LIST_HEAD(http_auth_basic_list);


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

#define set_auth_uid(e, u) \
	do { \
		int userlen = MIN((u)->userlen, HTTP_AUTH_USER_MAXLEN - 1); \
		if (userlen) \
			memcpy((e)->user, (u)->user, userlen); \
		(e)->user[userlen] = 0; \
	} while (0)

#define set_auth_passwd(e, u) \
	do { \
		int passwordlen = MIN((u)->passwordlen, HTTP_AUTH_PASSWORD_MAXLEN - 1); \
		if (passwordlen) \
			memcpy((e)->password, (u)->password, passwordlen); \
		(e)->password[passwordlen] = 0; \
	} while (0)

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

	entry->user = mem_alloc(HTTP_AUTH_USER_MAXLEN + HTTP_AUTH_PASSWORD_MAXLEN);
	if (!entry->user) {
		if (entry->realm) mem_free(entry->realm);
		mem_free(entry);
		return NULL;
	}
	entry->password = entry->user + HTTP_AUTH_USER_MAXLEN;
	set_auth_uid(entry, uri);
	set_auth_passwd(entry, uri);

	return entry;
}

/* Add a Basic Auth entry if needed. */
/* Returns the new entry or updates an existing one. Sets the @valid member if
 * updating is required so it can be tested if the user should be queried. */
struct http_auth_basic *
add_auth_entry(struct uri *uri, unsigned char *realm)
{
	struct http_auth_basic *entry;
	unsigned char *newurl = get_uri_string(uri, ~(URI_DATA | URI_POST));

	if (!newurl) return NULL;

	/* Is host/realm already known ? */
	entry = find_auth_entry(newurl, realm);
	if (entry) {
		mem_free(newurl);

		if (entry->blocked == 1) {
			/* Waiting for user/pass in dialog. */
			return NULL;
		}

		/* In order to use an existing entry it has to match exactly.
		 * This is done step by step. If something isn't equal the
		 * entry is updated and marked as invalid. */

		/* If only one realm is defined or they don't compare. */
		if ((!!realm ^ !!entry->realm)
		    || (realm && entry->realm && strcmp(realm, entry->realm))) {
			entry->valid = 0;
			if (entry->realm) {
				mem_free(entry->realm);
				entry->realm = NULL;
			}
			if (realm) {
				entry->realm = stracpy(realm);
				if (!entry->realm) {
					del_auth_entry(entry);
					return NULL;
				}
			}
		}

		if (!*entry->user || strlen(entry->user) != uri->userlen
		    || strncmp(entry->user, uri->user, uri->userlen)) {
			entry->valid = 0;
			set_auth_uid(entry, uri);
		}

		if (!*entry->password || strlen(entry->password) != uri->passwordlen
		    || strncmp(entry->password, uri->password, uri->passwordlen)) {
			entry->valid = 0;
			set_auth_passwd(entry, uri);
		}

	} else {
		/* Create a new entry. */
		entry = init_auth_entry(newurl, realm, uri);
		if (!entry) {
			mem_free(newurl);
			return NULL;
		}

		add_to_list(http_auth_basic_list, entry);
	}

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
	unsigned char *newurl = get_uri_string(uri, ~(URI_DATA | URI_POST));

	if (!newurl) return NULL;

	entry = find_auth_entry(newurl, NULL);
	mem_free(newurl);

	/* Check is user/pass info is in url. */
	if (uri->userlen || uri->passwordlen) {
		/* If there's no entry a new one is added else if the entry
		 * does not correspond to any existing one update it with the
		 * user and password from the uri. */
		/* FIXME BOOLEAN OVERLOAD! */
		if (!entry
		    || (auth_entry_has_userinfo(entry)
			&& strlen(entry->password) == uri->passwordlen
		        && strlen(entry->user) == uri->userlen
		        && !strncmp(entry->password, uri->password, uri->passwordlen)
		        && !strncmp(entry->user, uri->user, uri->userlen))) {

			entry = add_auth_entry(uri, NULL);
		}
	}

	/* No entry found. */
	if (!entry) return NULL;

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
	if (entry->url) mem_free(entry->url);
	if (entry->realm) mem_free(entry->realm);
	if (entry->user) mem_free(entry->user);
	/* if (entry->password) mem_free(entry->user); Allocated at the same
	 * time as user field, so no need to free it. */

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

struct http_auth_basic *
get_invalid_auth_entry(void)
{
	struct http_auth_basic *entry;

	foreach (entry, http_auth_basic_list)
		if (!entry->valid)
			return entry;

	return NULL;
}
