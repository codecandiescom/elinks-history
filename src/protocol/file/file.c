/* Internal "file" protocol implementation */
/* $Id: file.c,v 1.168 2004/05/29 19:17:02 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef HAVE_DIRENT_H
#include <dirent.h> /* OS/2 needs this after sys/types.h */
#endif
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
#include "util/memory.h"
#include "util/string.h"


/* Directory listing */
/* The stat_* functions set the various attributes for directory entries. */

static inline void
stat_type(struct string *string, struct stat *stp)
{
	unsigned char c = '?';

	if (stp) {
		if (S_ISDIR(stp->st_mode)) c = 'd';
		else if (S_ISREG(stp->st_mode)) c = '-';
#ifdef S_ISBLK
		else if (S_ISBLK(stp->st_mode)) c = 'b';
#endif
#ifdef S_ISCHR
		else if (S_ISCHR(stp->st_mode)) c = 'c';
#endif
#ifdef S_ISFIFO
		else if (S_ISFIFO(stp->st_mode)) c = 'p';
#endif
#ifdef S_ISLNK
		else if (S_ISLNK(stp->st_mode)) c = 'l';
#endif
#ifdef S_ISSOCK
		else if (S_ISSOCK(stp->st_mode)) c = 's';
#endif
#ifdef S_ISNWK
		else if (S_ISNWK(stp->st_mode)) c = 'n';
#endif
	}

	add_char_to_string(string, c);
}

static inline void
stat_mode(struct string *string, struct stat *stp)
{
#ifdef FS_UNIX_RIGHTS
	unsigned char rwx[10] = "---------";

	if (stp) {
		int mode = stp->st_mode;
		int shift;

		/* Set permissions attributes for user, group and other */
		for (shift = 0; shift <= 6; shift += 3) {
			int m = mode << shift;

			if (m & S_IRUSR) rwx[shift + 0] = 'r';
			if (m & S_IWUSR) rwx[shift + 1] = 'w';
			if (m & S_IXUSR) rwx[shift + 2] = 'x';
		}

#ifdef S_ISUID
		if (mode & S_ISUID)
			rwx[2] = (mode & S_IXUSR) ? 's' : 'S';
#endif
#ifdef S_ISGID
		if (mode & S_ISGID)
			rwx[5] = (mode & S_IXGRP) ? 's' : 'S';
#endif
#ifdef S_ISVTX
		if (mode & S_ISVTX)
			rwx[8] = (mode & S_IXOTH) ? 't' : 'T';
#endif
	}
	add_to_string(string, rwx);
#endif
	add_char_to_string(string, ' ');
}

static inline void
stat_links(struct string *string, struct stat *stp)
{
#ifdef FS_UNIX_HARDLINKS
	if (!stp) {
		add_to_string(string, "    ");
	} else {
		unsigned char lnk[64];

		ulongcat(lnk, NULL, stp->st_nlink, 3, ' ');
		add_to_string(string, lnk);
		add_char_to_string(string, ' ');
	}
#endif
}

static inline void
stat_user(struct string *string, struct stat *stp)
{
#ifdef FS_UNIX_USERS
	static unsigned char last_user[64];
	static int last_uid = -1;

	if (!stp) {
		add_to_string(string, "         ");
		return;
	}

	if (stp->st_uid != last_uid) {
		struct passwd *pwd = getpwuid(stp->st_uid);

		if (!pwd || !pwd->pw_name)
			/* ulongcat() can't pad from right. */
			sprintf(last_user, "%-8d", (int) stp->st_uid);
		else
			sprintf(last_user, "%-8.8s", pwd->pw_name);

		last_uid = stp->st_uid;
	}

	add_to_string(string, last_user);
	add_char_to_string(string, ' ');
#endif
}

static inline void
stat_group(struct string *string, struct stat *stp)
{
#ifdef FS_UNIX_USERS
	static unsigned char last_group[64];
	static int last_gid = -1;

	if (!stp) {
		add_to_string(string, "         ");
		return;
	}

	if (stp->st_gid != last_gid) {
		struct group *grp = getgrgid(stp->st_gid);

		if (!grp || !grp->gr_name)
			/* ulongcat() can't pad from right. */
			sprintf(last_group, "%-8d", (int) stp->st_gid);
		else
			sprintf(last_group, "%-8.8s", grp->gr_name);

		last_gid = stp->st_gid;
	}

	add_to_string(string, last_group);
	add_char_to_string(string, ' ');
#endif
}

static inline void
stat_size(struct string *string, struct stat *stp)
{
	if (!stp) {
		add_to_string(string, "         ");
	} else {
		unsigned char size[9];

		ulongcat(size, NULL, stp->st_size, 8, ' ');
		add_to_string(string, size);
		add_char_to_string(string, ' ');
	}
}

static inline void
stat_date(struct string *string, struct stat *stp)
{
#ifdef HAVE_STRFTIME
	if (stp) {
		time_t current_time = time(NULL);
		time_t when = stp->st_mtime;
		unsigned char *fmt;

		if (current_time > when + 6L * 30L * 24L * 60L * 60L
		    || current_time < when - 60L * 60L)
			fmt = "%b %e  %Y";
		else
			fmt = "%b %e %H:%M";

		add_date_to_string(string, fmt, &when);
		add_char_to_string(string, ' ');
		return;
	}
#endif
	add_to_string(string, "             ");
}


struct directory_entry {
	/* The various attribute info collected with the stat_* functions. */
	unsigned char *attrib;

	/* The full path of the dir entry. */
	unsigned char *name;
};

static int
compare_dir_entries(struct directory_entry *d1, struct directory_entry *d2)
{
	if (d1->name[0] == '.' && d1->name[1] == '.' && !d1->name[2]) return -1;
	if (d2->name[0] == '.' && d2->name[1] == '.' && !d2->name[2]) return 1;
	if (d1->attrib[0] == 'd' && d2->attrib[0] != 'd') return -1;
	if (d1->attrib[0] != 'd' && d2->attrib[0] == 'd') return 1;
	return strcmp(d1->name, d2->name);
}


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

/* This function decides whether a file should be shown in directory listing or
 * not. Returns according boolean value. */
static inline int
file_visible(unsigned char *name, int show_hidden_files, int is_root_directory)
{
	/* Always show everything not beginning with a dot. */
	if (name[0] != '.')
		return 1;

	/* Always hide the "." directory. */
	if (name[1] == '\0')
		return 0;

	/* Always show the ".." directory (but for root directory). */
	if (name[1] == '.' && name[2] == '\0')
		return !is_root_directory;

	/* Others like ".x" or "..foo" are shown if show_hidden_files
	 * == 1. */
	return show_hidden_files;
}

/* First information such as permissions is gathered for each directory entry.
 * All entries are then sorted and finally the sorted entries are added to the
 * @data->fragment one by one. */
static inline void
add_dir_entries(DIR *directory, unsigned char *dirpath, struct string *page)
{
	struct directory_entry *entries = NULL;
	int size = 0;
	struct dirent *entry;
	unsigned char dircolor[8];
	int show_hidden_files = get_opt_bool("protocol.file.show_hidden_files");
	int is_root_directory = dirpath[0] == '/' && !dirpath[1];

	/* Setup @dircolor so it's easy to check if we should color dirs. */
	if (get_opt_int("document.browse.links.color_dirs")) {
		color_to_string(get_opt_color("document.colors.dirs"),
				(unsigned char *) &dircolor);
	} else {
		dircolor[0] = 0;
	}

	while ((entry = readdir(directory))) {
		struct stat st, *stp;
		struct directory_entry *new_entries;
		unsigned char *name;
		struct string attrib;

		if (!file_visible(entry->d_name, show_hidden_files, is_root_directory))
			continue;

		new_entries = mem_realloc(entries, (size + 1) *
					  sizeof(struct directory_entry));
		if (!new_entries) continue;
		entries = new_entries;

		/* We allocate the full path because it is used in a few places
		 * which means less allocation although a bit more short term
		 * memory usage. */
		name = straconcat(dirpath, entry->d_name, NULL);
		if (!name) continue;

		if (!init_string(&attrib)) {
			mem_free(name);
			continue;
		}

#ifdef FS_UNIX_SOFTLINKS
		stp = (lstat(name, &st)) ? NULL : &st;
#else
		stp = (stat(name, &st)) ? NULL : &st;
#endif

		stat_type(&attrib, stp);
		stat_mode(&attrib, stp);
		stat_links(&attrib, stp);
		stat_user(&attrib, stp);
		stat_group(&attrib, stp);
		stat_size(&attrib, stp);
		stat_date(&attrib, stp);

		entries[size].name = name;
		entries[size].attrib = attrib.source;
		size++;
	}

	if (size) {
		int dirpathlen = strlen(dirpath);
		int i;

		qsort(entries, size, sizeof(struct directory_entry),
		      (int(*)(const void *, const void *))compare_dir_entries);

		for (i = 0; i < size; i++) {
			add_dir_entry(&entries[i], page, dirpathlen, dircolor);
			mem_free(entries[i].attrib);
			mem_free(entries[i].name);
		}
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

	if (get_opt_int_tree(cmdline_options, "anonymous")) {
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
