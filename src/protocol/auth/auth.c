/* HTTP Authentication support */
/* $Id: auth.c,v 1.23 2003/07/10 04:38:29 jonas Exp $ */

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
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


INIT_LIST_HEAD(http_auth_basic_list);

/* Returns a valid host url for http authentification or NULL. */
/* FIXME: This really belongs to url.c, but it would look alien there. */
static unsigned char *
get_auth_url(struct uri *uri)
{
	unsigned char *str = init_str();
	int len = 0;

	if (!str) return NULL;
	assert(uri->protocol && uri->protocollen && uri->host && uri->hostlen);
	if_assert_failed { mem_free(str); return NULL; }

	add_bytes_to_str(&str, &len, uri->protocol, uri->protocollen);
	add_to_str(&str, &len, "://");
	add_bytes_to_str(&str, &len, uri->host, uri->hostlen);
	add_chr_to_str(&str, &len, ':');

	if (uri->port && uri->portlen) {
		add_bytes_to_str(&str, &len, uri->port, uri->portlen);
	} else {
		/* Should user protocols ports be configurable? */
		enum protocol protocol = check_protocol(uri->protocol,
							uri->protocollen);
		int port = get_protocol_port(protocol);

		/* RFC2616 section 3.2.2:
		 * "If the port is empty or not given, port 80 is assumed." */
		/* Port 0 comes from user protocol backend so be httpcentric. */
		add_num_to_str(&str, &len, (port != 0 ? port : 80));
	}

	return str;
}


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
	unsigned char *user = memacpy(uri->user, uri->userlen);
	unsigned char *pass = memacpy(uri->password, uri->passwordlen);
	unsigned char *newurl = get_auth_url(uri);
	int ret = ADD_AUTH_ERROR;

	if (!newurl || !user || !pass) goto end;

	/* Is host/realm already known ? */
	entry = find_auth_entry(newurl, realm);
	if (entry) {
		/* Found an entry. */
		if (entry->blocked == 1) {
			/* Waiting for user/pass in dialog. */
			ret = ADD_AUTH_EXIST;
			goto end;
		}

		/* If we have user/pass info then check if identical to
		 * those in entry. */
		if ((*user || *pass) && entry->uid && entry->passwd) {
			if (((!realm && !entry->realm)
			    || (realm && entry->realm && !strcmp(realm, entry->realm)))
			    && !strcmp(user, entry->uid)
			    && !strcmp(pass, entry->passwd)) {
				/* Same host/realm/pass/user. */
				ret = ADD_AUTH_EXIST;
				goto end;
			}
		}

		/* Delete entry and re-create it... */
		/* FIXME: Could be better... */
		del_auth_entry(entry);
	}

	/* Create a new entry. */
	entry = mem_calloc(1, sizeof(struct http_auth_basic));
	if (!entry) goto end;

	entry->url = newurl;
	entry->url_len = strlen(entry->url); /* FIXME: Not really needed. */

        if (realm) {
		/* Copy realm value. */
		entry->realm = stracpy(realm);
		if (!entry->realm) {
			mem_free(entry);
			goto end;
		}
	}

	if (*user || *pass) {
		/* Copy user and pass info if any in passed url. */
		entry->uid = mem_alloc(MAX_UID_LEN);
		if (!entry->uid) {
			mem_free(entry);
			goto end;
		}
		safe_strncpy(entry->uid, user, MAX_UID_LEN);

		entry->passwd = mem_alloc(MAX_PASSWD_LEN);
		if (!entry->passwd) {
			mem_free(entry);
			goto end;
		}
		safe_strncpy(entry->passwd, pass, MAX_PASSWD_LEN);

		ret = ADD_AUTH_NONE; /* Entry added with user/pass from url. */
	}

	add_to_list(http_auth_basic_list, entry);

	if (ret != ADD_AUTH_NONE) ret = ADD_AUTH_NEW; /* Entry added. */

end:
	if (ret == ADD_AUTH_ERROR || ret == ADD_AUTH_EXIST) {
		if (newurl) mem_free(newurl);
	}

	if (user) mem_free(user);
	if (pass) mem_free(pass);

	return ret;
}

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
	unsigned char *newurl = get_auth_url(uri);
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
