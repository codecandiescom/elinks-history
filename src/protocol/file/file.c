/* Internal "file" protocol implementation */
/* $Id: file.c,v 1.136 2003/12/04 11:11:04 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
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
#include "osdep/osdep.h"

#ifdef ELINKS_LOCAL_CGI
#include "protocol/file/cgi.h"
#endif /* ELINKS_LOCAL_CGI */

#include "protocol/file/file.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "util/conv.h"
#include "util/encoding.h"
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

	if (stp->st_uid != last_uid || last_uid == -1) {
		struct passwd *pwd = getpwuid(stp->st_uid);

		if (!pwd || !pwd->pw_name)
			/* ulongcat() can't pad from right. */
			sprintf(last_user, "%-8d", stp->st_uid);
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

	if (stp->st_gid != last_gid || last_gid == -1) {
		struct group *grp = getgrgid(stp->st_gid);

		if (!grp || !grp->gr_name)
			/* ulongcat() can't pad from right. */
			sprintf(last_group, "%-8d", stp->st_gid);
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
		struct tm *when_local = localtime(&when);
		unsigned char *fmt;
		unsigned char str[13];
		int wr;

		if (current_time > when + 6L * 30L * 24L * 60L * 60L
		    || current_time < when - 60L * 60L)
			fmt = "%b %e  %Y";
		else
			fmt = "%b %e %H:%M";

		wr = strftime(str, sizeof(str), fmt, when_local);

		while (wr < sizeof(str) - 1) str[wr++] = ' ';
		str[sizeof(str) - 1] = '\0';
		add_to_string(string, str);
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
file_visible(unsigned char *name, int show_hidden_files)
{
	/* Always show everything not beginning with a dot. */
	if (name[0] != '.')
		return 1;

	/* Always hide the "." directory. */
	if (name[1] == '\0')
		return 0;

	/* Always show the ".." directory. */
	if (name[1] == '.' && name[2] == '\0')
		return 1;

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

		if (!file_visible(entry->d_name, show_hidden_files))
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
	if (entries) mem_free(entries);
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

/* Tries to open @prefixname with each of the supported encoding extensions
 * appended. */
static inline enum stream_encoding
try_encoding_extensions(unsigned char *filename, int filenamelen, int *fd)
{
	int maxlen = MAX_STR_LEN - filenamelen - 1;
	unsigned char *filenamepos = filename + filenamelen;
	int encoding;

	/* No file of that name was found, try some others names. */
	for (encoding = 1; encoding < ENCODINGS_KNOWN; encoding++) {
		unsigned char **ext = listext_encoded(encoding);

		for (; ext && *ext; ext++) {
			int extlen = strlen(*ext);

			if (extlen > maxlen) continue;

			memcpy(filenamepos, *ext, extlen + 1);

			/* We try with some extensions. */
			*fd = open(filename, O_RDONLY | O_NOCTTY);

			if (*fd >= 0)
				/* Ok, found one, use it. */
				return encoding;
		}
	}

	filename[filenamelen + 1] = 0;
	return ENCODING_NONE;
}

/* Reads the file from @stream in chunks of size @readsize. */
/* Returns a connection state. S_OK if all is well. */
static inline enum connection_state
read_file(struct stream_encoded *stream, int readsize, struct string *page)
{
	/* + 1 is there because of bug in Linux. Read returns -EACCES when
	 * reading 0 bytes to invalid address */
	unsigned char *fragment = mem_alloc(readsize + 1);
	int fragmentlen = 0;
	int readlen;

	if (!fragment)
		return S_OUT_OF_MEM;

	/* We read with granularity of stt.st_size (given as @readsize) - this
	 * does best job for uncompressed files, and doesn't hurt for
	 * compressed ones anyway - very large files usually tend to inflate
	 * fast anyway. At least I hope ;).  --pasky */
	while ((readlen = read_encoded(stream, fragment + fragmentlen, readsize))) {
		unsigned char *tmp;

		if (readlen < 0) {
			/* FIXME: We should get the correct error value.
			 * But it's I/O error in 90% of cases anyway.. ;)
			 * --pasky */
			mem_free(fragment);
			return (enum connection_state) -errno;
		}

		fragmentlen += readlen;

#if 0
		/* This didn't work so well as it should (I had to implement
		 * end of stream handling to bzip2 anyway), so I rather
		 * disabled this. */
		if (readlen < readsize) {
			/* This is much safer. It should always mean that we
			 * already read everything possible, and it permits us
			 * more elegant of handling end of file with bzip2. */
			break;
		}
#endif

		tmp = mem_realloc(fragment, fragmentlen + readsize);
		if (!tmp) {
			mem_free(fragment);
			return S_OUT_OF_MEM;
		}

		fragment = tmp;
	}

	fragment[fragmentlen] = '\0'; /* NULL-terminate just in case */

	page->source = fragment;
	page->length = fragmentlen;
	/* XXX BAD practice that should be changed. --jonas */
	set_string_magic(page);
	return S_OK;
}


/* To reduce redundant error handling code [calls to abort_conn_with_state()]
 * most of the function is build around conditions that will assign the error
 * code to @state if anything goes wrong. The rest of the function will then just
 * do the necessary cleanups. If all works out we end up with @state being S_OK
 * resulting in a cache entry being created with the fragment data generated by
 * either reading the file content or listing a directory. */
static void
file_func(struct connection *connection)
{
	unsigned char *redirect = NULL;
	unsigned char filename[MAX_STR_LEN];
	int filenamelen = connection->uri.datalen;
	DIR *directory;
	struct string page;
	enum connection_state state;
	unsigned char *head = NULL;

	if (get_opt_int_tree(cmdline_options, "anonymous")
	    || filenamelen > MAX_STR_LEN - 1 || filenamelen <= 0) {
		abort_conn_with_state(connection, S_BAD_URL);
		return;
	}

#ifdef ELINKS_LOCAL_CGI
	if (!execute_cgi(connection)) return;
#endif /* ELINKS_LOCAL_CGI */

	/* This function works on already simplified file-scheme URI pre-chewed
	 * by transform_file_url(). By now, the function contains no hostname
	 * part anymore, possibly relative path is converted to an absolute one
	 * and uri.data is just the final path to file/dir we should try to
	 * show. */

	safe_strncpy(filename, connection->uri.data, filenamelen + 1);
	decode_uri_string(filename);
	filenamelen = strlen(filename);

	directory = opendir(filename);
	if (directory) {
		/* In order for global history and directory listing to
		 * function properly the directory url must end with a
		 * directory separator. */
		if (filename[0] && !dir_sep(filename[filenamelen - 1])) {
			redirect = straconcat(struri(connection->uri), "/", NULL);
			state = S_OK;
		} else {
			state = list_directory(directory, filename, &page);
			head = "\r\nContent-Type: text/html\r\n";
		}

		closedir(directory);

	} else {
		struct stream_encoded *stream;
		struct stat stt;
		enum stream_encoding encoding = ENCODING_NONE;
		int fd = open(filename, O_RDONLY | O_NOCTTY);
		int saved_errno = errno;

		if (fd == -1
		    && get_opt_bool("protocol.file.try_encoding_extensions")) {
			encoding = try_encoding_extensions(filename,
							   filenamelen, &fd);
		} else if (fd != -1) {
			encoding = guess_encoding(filename);
		}

		if (fd == -1) {
			abort_conn_with_state(connection, -saved_errno);
			return;
		}

		/* Some file was opened so let's get down to bi'ness */
		set_bin(fd);

		/* Do all the necessary checks before trying to read the file.
		 * @state code is used to block further progress. */
		if (fstat(fd, &stt)) {
			state = -errno;

		} else if (!S_ISREG(stt.st_mode) && encoding != ENCODING_NONE) {
			/* We only want to open regular encoded files. */
			state = -saved_errno;

		} else if (!S_ISREG(stt.st_mode) &&
			   !get_opt_int("protocol.file.allow_special_files")) {
			state = S_FILE_TYPE;

		} else if (!(stream = open_encoded(fd, encoding))) {
			state = S_OUT_OF_MEM;

		} else {
			state = read_file(stream, stt.st_size, &page);
			close_encoded(stream);
			head = "";
		}

		close(fd);
	}

	if (state == S_OK) {
		struct cache_entry *cache;

		/* Try to add fragment data to the connection cache if either
		 * file reading or directory listing worked out ok. */
		cache = get_cache_entry(struri(connection->uri));
		if (!cache) {
			if (!redirect) done_string(&page);
			state = S_OUT_OF_MEM;

		} else if (redirect) {
			/* Setup redirect to directory with '/' appended */
			if (cache->redirect) mem_free(cache->redirect);
			cache->redirect_get = 1;
			cache->redirect = redirect;
			cache->incomplete = 0;
			connection->cache = cache;

		} else {
			assert(head);
			if_assert_failed {
				done_string(&page);
				abort_conn_with_state(connection, S_INTERNAL);
				return;
			}

			/* Setup file read or directory listing for viewing. */
			if (cache->head) mem_free(cache->head);
			cache->head = stracpy(head);
			cache->incomplete = 0;
			connection->cache = cache;

			add_fragment(cache, 0, page.source, page.length);
			truncate_entry(cache, page.length, 1);
			done_string(&page);
		}
	}

	abort_conn_with_state(connection, state);
}

struct protocol_backend file_protocol_backend = {
	/* name: */			"file",
	/* port: */			0,
	/* handler: */			file_func,
	/* external_handler: */		NULL,
	/* free_syntax: */		1,
	/* need_slashes: */		1,
	/* need_slash_after_host: */	0,
};
