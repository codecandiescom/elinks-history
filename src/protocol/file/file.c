/* Internal "file" protocol implementation */
/* $Id: file.c,v 1.171 2004/07/18 01:50:02 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef TIME_WITH_SYS_TIME
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#else
#if defined(TM_IN_SYS_TIME) && defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#elif defined(HAVE_TIME_H)
#include <time.h>
#endif
#endif

#include "elinks.h"

#include "config/options.h"
#include "cache/cache.h"
#include "encoding/encoding.h"
#include "protocol/file/cgi.h"
#include "protocol/file/file.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "util/conv.h"
#include "util/file.h"
#include "util/memory.h"
#include "util/string.h"


/* Directory listing */

/* Based on the @entry attributes and file-/dir-/linkname is added to the @data
 * fragment. */
static inline void
add_dir_entry(struct directory_entry *entry, struct string *page,
	      int pathlen, unsigned char *dircolor)
{
	unsigned char *lnk = NULL;
	struct string html_encoded_name;
	struct string uri_encoded_name;

	if (!init_string(&html_encoded_name)) return;
	if (!init_string(&uri_encoded_name)) {
		done_string(&html_encoded_name);
		return;
	}

	encode_uri_string(&uri_encoded_name, entry->name + pathlen);
	add_html_to_string(&html_encoded_name, entry->name + pathlen,
			   strlen(entry->name) - pathlen);

	/* add_to_string(&fragment, &fragmentlen, "   "); */
	add_html_to_string(page, entry->attrib, strlen(entry->attrib));
	add_to_string(page, "<a href=\"");
	add_string_to_string(page, &uri_encoded_name);

	if (entry->attrib[0] == 'd') {
		add_char_to_string(page, '/');

#ifdef FS_UNIX_SOFTLINKS
	} else if (entry->attrib[0] == 'l') {
		struct stat st;
		unsigned char buf[MAX_STR_LEN];
		int readlen = readlink(entry->name, buf, MAX_STR_LEN);

		if (readlen > 0 && readlen != MAX_STR_LEN) {
			buf[readlen] = '\0';
			lnk = straconcat(" -> ", buf, NULL);
		}

		if (!stat(entry->name, &st) && S_ISDIR(st.st_mode))
			add_char_to_string(page, '/');
#endif
	}

	add_to_string(page, "\">");

	if (entry->attrib[0] == 'd' && *dircolor) {
		/* The <b> is for the case when use_document_colors is off. */
		string_concat(page, "<font color=\"", dircolor, "\"><b>", NULL);
	}

	add_string_to_string(page, &html_encoded_name);
	done_string(&uri_encoded_name);
	done_string(&html_encoded_name);

	if (entry->attrib[0] == 'd' && *dircolor) {
		add_to_string(page, "</b></font>");
	}

	add_to_string(page, "</a>");
	if (lnk) {
		add_html_to_string(page, lnk, strlen(lnk));
		mem_free(lnk);
	}

	add_char_to_string(page, '\n');
}

/* First information such as permissions is gathered for each directory entry.
 * All entries are then sorted and finally the sorted entries are added to the
 * @data->fragment one by one. */
static inline void
add_dir_entries(DIR *directory, unsigned char *dirpath, struct string *page)
{
	struct directory_entry *entries;
	unsigned char dircolor[8];
	int show_hidden_files = get_opt_bool("protocol.file.show_hidden_files");
	int dirpathlen = strlen(dirpath);
	int i;

	/* Setup @dircolor so it's easy to check if we should color dirs. */
	if (get_opt_int("document.browse.links.color_dirs")) {
		color_to_string(get_opt_color("document.colors.dirs"),
				(unsigned char *) &dircolor);
	} else {
		dircolor[0] = 0;
	}

	entries = get_directory_entries(directory, dirpath, show_hidden_files);
	if (!entries) return;

	for (i = 0; entries[i].name; i++) {
		add_dir_entry(&entries[i], page, dirpathlen, dircolor);
		mem_free(entries[i].attrib);
		mem_free(entries[i].name);
	}

	/* We may have allocated space for entries but added none. */
	mem_free_if(entries);
}

/* Generates a HTML page listing the content of @directory with the path
 * @dirpath. */
/* Returns a connection state. S_OK if all is well. */
static inline enum connection_state
list_directory(DIR *directory, unsigned char *dirpath, struct string *page)
{
	unsigned char *slash = dirpath;
	unsigned char *pslash = ++slash;

	if (!init_string(page)) return S_OUT_OF_MEM;

	add_to_string(page, "<html>\n<head><title>");
	add_html_to_string(page, dirpath, strlen(dirpath));
	add_to_string(page, "</title></head>\n<body>\n<h2>Directory /");

	/* Make the directory path with links to each subdir. */
	while ((slash = strchr(slash, '/'))) {
		*slash = 0;
		add_to_string(page, "<a href=\"");
		/* FIXME: htmlesc? At least we should escape quotes. --pasky */
		add_to_string(page, dirpath);
		add_to_string(page, "/\">");
		add_html_to_string(page, pslash, strlen(pslash));
		add_to_string(page, "</a>/");
		*slash = '/';
		pslash = ++slash;
	}

	add_to_string(page, "</h2>\n<pre>");
	add_dir_entries(directory, dirpath, page);
	add_to_string(page, "</pre>\n<hr>\n</body>\n</html>\n");
	return S_OK;
}


/* File reading */

/* To reduce redundant error handling code [calls to abort_conn_with_state()]
 * most of the function is build around conditions that will assign the error
 * code to @state if anything goes wrong. The rest of the function will then just
 * do the necessary cleanups. If all works out we end up with @state being S_OK
 * resulting in a cache entry being created with the fragment data generated by
 * either reading the file content or listing a directory. */
void
file_protocol_handler(struct connection *connection)
{
	unsigned char *redirect_location = NULL;
	DIR *directory;
	struct string page, name;
	enum connection_state state;
	unsigned char *head = "";

	if (get_cmd_opt_int("anonymous")) {
		/* FIXME: Better connection_state ;-) */
		abort_conn_with_state(connection, S_BAD_URL);
		return;
	}

#ifdef CONFIG_CGI
	if (!execute_cgi(connection)) return;
#endif /* CONFIG_CGI */

	/* This function works on already simplified file-scheme URI pre-chewed
	 * by transform_file_url(). By now, the function contains no hostname
	 * part anymore, possibly relative path is converted to an absolute one
	 * and uri->data is just the final path to file/dir we should try to
	 * show. */

	if (!init_string(&name)
	    || !add_uri_to_string(&name, connection->uri, URI_PATH)) {
		done_string(&name);
		abort_conn_with_state(connection, S_OUT_OF_MEM);
		return;
	}

	decode_uri_string(name.source);
	name.length = strlen(name.source);

	directory = opendir(name.source);
	if (directory) {
		/* In order for global history and directory listing to
		 * function properly the directory url must end with a
		 * directory separator. */
		if (name.source[0] && !dir_sep(name.source[name.length - 1])) {
			redirect_location = "/";
			state = S_OK;
		} else {
			state = list_directory(directory, name.source, &page);
			head = "\r\nContent-Type: text/html\r\n";
		}

		closedir(directory);

	} else {
		state = read_encoded_file(&name, &page);
	}

	done_string(&name);

	if (state == S_OK) {
		struct cache_entry *cached;

		/* Try to add fragment data to the connection cache if either
		 * file reading or directory listing worked out ok. */
		cached = connection->cached = get_cache_entry(connection->uri);
		if (!connection->cached) {
			if (!redirect_location) done_string(&page);
			state = S_OUT_OF_MEM;

		} else if (redirect_location) {
			if (!redirect_cache(cached, redirect_location, 1, 0))
				state = S_OUT_OF_MEM;

		} else {
			/* Setup file read or directory listing for viewing. */
			mem_free_set(&cached->head, stracpy(head));
			cached->incomplete = 0;

			add_fragment(cached, 0, page.source, page.length);
			truncate_entry(cached, page.length, 1);
			done_string(&page);
		}
	}

	abort_conn_with_state(connection, state);
}
