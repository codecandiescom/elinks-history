/* HTTP Authentication support */
/* $Id: auth.c,v 1.54 2003/07/13 12:57:53 jonas Exp $ */

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

#define min(x, y) ((x) < (y) ? (x) : (y))

#define set_auth_uid(e, u) \
	do { \
		int uidlen = min((u)->userlen, MAX_UID_LEN); \
		if (uidlen) \
			memcpy((e)->uid, (u)->user, uidlen); \
		(e)->uid[uidlen] = 0; \
	} while (0)

#define set_auth_passwd(e, u) \
	do { \
		int passwdlen = min((u)->passwordlen, MAX_PASSWD_LEN); \
		if (passwdlen) \
			memcpy((e)->passwd, (u)->password, passwdlen); \
		(e)->passwd[passwdlen] = 0; \
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

	entry->uid = mem_alloc(MAX_UID_LEN + MAX_PASSWD_LEN);
	if (!entry->uid) {
		if (entry->realm) mem_free(entry->realm);
		mem_free(entry);
		return NULL;
	}
	entry->passwd = entry->uid + MAX_UID_LEN;
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
	unsigned char *newurl = get_uri_string(uri, URI_DATA|URI_POST);

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

		if (!*entry->uid || strlen(entry->uid) != uri->userlen
		    || strncmp(entry->uid, uri->user, uri->userlen)) {
			entry->valid = 0;
			set_auth_uid(entry, uri);
		}

		if (!*entry->passwd || strlen(entry->passwd) != uri->passwordlen
		    || strncmp(entry->passwd, uri->password, uri->passwordlen)) {
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

#undef min

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
	unsigned char *uid, *ret;
	unsigned char *newurl = get_uri_string(uri, URI_DATA|URI_POST);

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
			&& strlen(entry->passwd) == uri->passwordlen
		        && strlen(entry->uid) == uri->userlen
		        && !strncmp(entry->passwd, uri->password, uri->passwordlen)
		        && !strncmp(entry->uid, uri->user, uri->userlen))) {

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
	uid = straconcat(entry->uid, ":", entry->passwd, NULL);
	if (!uid) return NULL;
	ret = base64_encode(uid);
	mem_free(uid);

	return ret;
}

/* Delete an entry from auth list. */
void
del_auth_entry(struct http_auth_basic *entry)
{
	if (entry->url) mem_free(entry->url);
	if (entry->realm) mem_free(entry->realm);
	if (entry->uid) mem_free(entry->uid);
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
