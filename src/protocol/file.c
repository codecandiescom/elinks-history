/* Internal "file" protocol implementation */
/* $Id: file.c,v 1.63 2003/06/23 02:21:27 jonas Exp $ */

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
#include "document/cache.h"
#include "protocol/file.h"
#include "protocol/url.h"
#include "sched/sched.h"
#include "util/conv.h"
#include "util/encoding.h"
#include "util/memory.h"
#include "util/string.h"


#ifdef FS_UNIX_RIGHTS
static inline void
setrwx(int m, unsigned char *p)
{
	if (m & S_IRUSR) p[0] = 'r';
	if (m & S_IWUSR) p[1] = 'w';
	if (m & S_IXUSR) p[2] = 'x';
}


static inline void
setst(int m, unsigned char *p)
{
#ifdef S_ISUID
	if (m & S_ISUID) {
		p[2] = 'S';
		if (m & S_IXUSR) p[2] = 's';
	}
#endif
#ifdef S_ISGID
	if (m & S_ISGID) {
		p[5] = 'S';
		if (m & S_IXGRP) p[5] = 's';
	}
#endif
#ifdef S_ISVTX
	if (m & S_ISVTX) {
		p[8] = 'T';
		if (m & S_IXOTH) p[8] = 't';
	}
#endif
}
#endif


static void
stat_mode(unsigned char **p, int *l, struct stat *stp)
{
	unsigned char c = '?';

	if (stp) {
		if (0);
#ifdef S_ISBLK
		else if (S_ISBLK(stp->st_mode)) c = 'b';
#endif
#ifdef S_ISCHR
		else if (S_ISCHR(stp->st_mode)) c = 'c';
#endif
		else if (S_ISDIR(stp->st_mode)) c = 'd';
		else if (S_ISREG(stp->st_mode)) c = '-';
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
	add_chr_to_str(p, l, c);
#ifdef FS_UNIX_RIGHTS
	{
		unsigned char rwx[10] = "---------";

		if (stp) {
			int mode = stp->st_mode;

			setrwx(mode << 0, &rwx[0]);
			setrwx(mode << 3, &rwx[3]);
			setrwx(mode << 6, &rwx[6]);
			setst(mode, rwx);
		}
		add_to_str(p, l, rwx);
	}
#endif
	add_chr_to_str(p, l, ' ');
}


static void
stat_links(unsigned char **p, int *l, struct stat *stp)
{
#ifdef FS_UNIX_HARDLINKS
	unsigned char lnk[64];

	if (!stp) add_to_str(p, l, "    ");
	else {
		ulongcat(lnk, NULL, stp->st_nlink, 3, ' ');
		add_to_str(p, l, lnk);
		add_chr_to_str(p, l, ' ');
	}
#endif
}


static int last_uid = -1;
static int last_gid = -1;


static void
stat_user(unsigned char **p, int *l, struct stat *stp, int g)
{
#ifdef FS_UNIX_USERS
	int id;
	unsigned char *pp;
	int i;

	if (!stp) {
		add_to_str(p, l, "         ");
		return;
	}

	if (!g) {
		static unsigned char last_user[64];
		struct passwd *pwd;

		id = stp->st_uid;
		pp = last_user;
		if (id == last_uid && last_uid != -1)
			goto end;

		pwd = getpwuid(id);
		if (!pwd || !pwd->pw_name)
			ulongcat(pp, NULL, id, 8, 0);
		else
			sprintf(pp, "%.8s", pwd->pw_name);
		last_uid = id;

	} else {
		static unsigned char last_group[64];
		struct group *grp;


		id = stp->st_gid;
		pp = last_group;
		if (id == last_gid && last_gid != -1)
			goto end;

		grp = getgrgid(id);
		if (!grp || !grp->gr_name)
			ulongcat(pp, NULL, id, 8, 0);
		else
			sprintf(pp, "%.8s", grp->gr_name);
		last_gid = id;

	}

end:
	add_to_str(p, l, pp);
	for (i = strlen(pp); i < 8; i++) add_chr_to_str(p, l, ' ');
	add_chr_to_str(p, l, ' ');
#endif
}


static void
stat_size(unsigned char **p, int *l, struct stat *stp)
{
	if (!stp) {
		add_to_str(p, l, "         ");
	} else {
		unsigned char size[9];

		ulongcat(size, NULL, stp->st_size, 8, ' ');
		add_to_str(p, l, size);
		add_chr_to_str(p, l, ' ');
	}
}


static void
stat_date(unsigned char **p, int *l, struct stat *stp)
{
	time_t current_time = time(NULL);
	time_t when;
	struct tm *when_local;
	unsigned char *fmt;
	unsigned char str[13];
	int wr;

	if (!stp) {
		add_to_str(p, l, "             ");
		return;
	}

	when = stp->st_mtime;
	when_local = localtime(&when);

	if (current_time > when + 6L * 30L * 24L * 60L * 60L
	    || current_time < when - 60L * 60L)
		fmt = "%b %e  %Y";
	else
		fmt = "%b %e %H:%M";

#ifdef HAVE_STRFTIME
	wr = strftime(str, sizeof(str), fmt, when_local);
#else
	wr = 0;
#endif

	while (wr < sizeof(str) - 1) str[wr++] = ' ';
	str[sizeof(str) - 1] = '\0';
	add_to_str(p, l, str);
	add_chr_to_str(p, l, ' ');
}


struct dirs {
	unsigned char *s;
	unsigned char *f;
};


static int
comp_de(struct dirs *d1, struct dirs *d2)
{
	if (d1->f[0] == '.' && d1->f[1] == '.' && !d1->f[2]) return -1;
	if (d2->f[0] == '.' && d2->f[1] == '.' && !d2->f[2]) return 1;
	if (d1->s[0] == 'd' && d2->s[0] != 'd') return -1;
	if (d1->s[0] != 'd' && d2->s[0] == 'd') return 1;
	return strcmp(d1->f, d2->f);
}

struct file_info {
	unsigned char *head;
	unsigned char *fragment;
	int fragmentlen;
};

/* Generates a HTML page listing the content of @directory with the path
 * @filename. */
/* Returns a connection state. S_OK if all is well. */
static int
list_directory(DIR *directory, unsigned char *filename, struct file_info *info)
{
	struct dirs *dir = NULL;
	int dirl = 0;
	int i;
	struct dirent *de;
	unsigned char dircolor[8];
	int colorize_dir = get_opt_int("document.browse.links.color_dirs");
	int show_hidden_files = get_opt_bool("protocol.file.show_hidden_files");
	unsigned char *fragment;
	int fragmentlen;

	if (colorize_dir) {
		color_to_string((struct rgb *) get_opt_ptr("document.colors.dirs"),
			(unsigned char *) &dircolor);
	}

	last_uid = -1;
	last_gid = -1;
	fragment = init_str();
	fragmentlen = 0;

	if (!fragment) return S_OUT_OF_MEM;

	add_to_str(&fragment, &fragmentlen, "<html>\n<head><title>");
	add_htmlesc_str(&fragment, &fragmentlen, filename, strlen(filename));
	add_to_str(&fragment, &fragmentlen, "</title></head>\n<body>\n<h2>Directory ");
	{
		unsigned char *pslash, *slash = filename - 1;

		while (pslash = ++slash, slash = strchr(slash, '/')) {
			if (slash == filename) {
				add_chr_to_str(&fragment, &fragmentlen, '/');
				continue;
			}

			slash[0] = 0;
			add_to_str(&fragment, &fragmentlen, "<a href=\"");
			/* FIXME: htmlesc? At least we should escape quotes. --pasky */
			add_to_str(&fragment, &fragmentlen, filename);
			add_chr_to_str(&fragment, &fragmentlen, '/');
			add_to_str(&fragment, &fragmentlen, "\">");
			add_htmlesc_str(&fragment, &fragmentlen, pslash, strlen(pslash));
			add_to_str(&fragment, &fragmentlen, "</a>");
			add_chr_to_str(&fragment, &fragmentlen, '/');
			slash[0] = '/';
		}
	}
	add_to_str(&fragment, &fragmentlen, "</h2>\n<pre>");

	while ((de = readdir(directory))) {
		struct stat st, *stp;
		unsigned char **p;
		int l;
		struct dirs *nd;
		unsigned char *n;

		/* Always show "..", always hide ".", others like ".x" are shown if
		 * show_hidden_files = 1 */
		if (de->d_name[0] == '.' && !(de->d_name[1] == '.' && de->d_name[2] == '\0'))
			if (!show_hidden_files || de->d_name[1] == '\0') continue;

		nd = mem_realloc(dir, (dirl + 1) * sizeof(struct dirs));
		if (!nd) continue;

		dir = nd;
		dir[dirl].f = stracpy(de->d_name);
		if (!dir[dirl].f) continue;

		p = &dir[dirl++].s;
		*p = init_str();
		if (!*p) continue;

		l = 0;
		n = stracpy(filename);
		if (!n) continue;

		add_to_strn(&n, de->d_name);
#ifdef FS_UNIX_SOFTLINKS
		if (lstat(n, &st))
#else
			if (stat(n, &st))
#endif
				stp = NULL;
			else
				stp = &st;

		mem_free(n);
		stat_mode(p, &l, stp);
		stat_links(p, &l, stp);
		stat_user(p, &l, stp, 0);
		stat_user(p, &l, stp, 1);
		stat_size(p, &l, stp);
		stat_date(p, &l, stp);
	}

	if (dirl) qsort(dir, dirl, sizeof(struct dirs),
		(int(*)(const void *, const void *))comp_de);

	for (i = 0; i < dirl; i++) {
		unsigned char *lnk = NULL;

#ifdef FS_UNIX_SOFTLINKS
		if (dir[i].s[0] == 'l') {

			unsigned char *buf = NULL;
			int size = 0;
			int rl = -1;
			unsigned char *n = init_str();
			int nl = 0;

			if (!n) continue;
			add_to_str(&n, &nl, filename);
			add_htmlesc_str(&n, &nl,
				dir[i].f, strlen(dir[i].f));
			do {
				if (buf) mem_free(buf);
				size += ALLOC_GR;
				buf = mem_alloc(size);
				if (!buf) break;
				rl = readlink(n, buf, size);
			} while (rl == size);

			mem_free(n);
			if (buf) {
				if (rl != -1) {
					buf[rl] = '\0';

					lnk = buf;
				} else {
					mem_free(buf);
				}
			}
		}
#endif
		/* add_to_str(&fragment, &fragmentlen, "   "); */
		add_htmlesc_str(&fragment, &fragmentlen,
			dir[i].s, strlen(dir[i].s));
		add_to_str(&fragment, &fragmentlen, "<a href=\"");
		add_htmlesc_str(&fragment, &fragmentlen,
			dir[i].f, strlen(dir[i].f));
		if (dir[i].s[0] == 'd') {
			add_chr_to_str(&fragment, &fragmentlen, '/');
		} else if (lnk) {
			struct stat st;
			unsigned char *n = init_str();
			int nl = 0;

			if (n) {
				add_to_str(&n, &nl, filename);
				add_htmlesc_str(&n, &nl,
					dir[i].f, strlen(dir[i].f));
				if (!stat(n, &st) && S_ISDIR(st.st_mode))
					add_chr_to_str(&fragment, &fragmentlen, '/');
				mem_free(n);
			}
		}
		add_to_str(&fragment, &fragmentlen, "\">");

		if (dir[i].s[0] == 'd' && colorize_dir) {
			/* The <b> is here for the case when we've
			 * use_document_colors off. */
			add_to_str(&fragment, &fragmentlen, "<font color=\"");
			add_to_str(&fragment, &fragmentlen, dircolor);
			add_to_str(&fragment, &fragmentlen, "\"><b>");
		}

		add_htmlesc_str(&fragment, &fragmentlen,
			dir[i].f, strlen(dir[i].f));

		if (dir[i].s[0] == 'd' && colorize_dir) {
			add_to_str(&fragment, &fragmentlen, "</b></font>");
		}

		add_to_str(&fragment, &fragmentlen, "</a>");
		if (lnk) {
			add_to_str(&fragment, &fragmentlen, " -> ");
			add_htmlesc_str(&fragment, &fragmentlen, lnk, strlen(lnk));
			mem_free(lnk);
		}

		add_chr_to_str(&fragment, &fragmentlen, '\n');
	}

	for (i = 0; i < dirl; i++) {
		if (dir[i].s) mem_free(dir[i].s);
		if (dir[i].f) mem_free(dir[i].f);
	}
	mem_free(dir);

	add_to_str(&fragment, &fragmentlen, "</pre>\n<hr>\n</body>\n</html>\n");

	info->fragment = fragment;
	info->fragmentlen = fragmentlen;
	info->head = stracpy("\r\nContent-Type: text/html\r\n");
	return S_OK;
}

static enum stream_encoding
try_encoding_extensions(unsigned char *prefixname, int *fd)
{
	unsigned char filename[MAX_STR_LEN];
	int filenamelen = strlen(prefixname);
	int maxlen = MAX_STR_LEN - filenamelen;
	unsigned char *filenamepos = filename + filenamelen;
	int encoding;

	memcpy(filename, prefixname, filenamelen);

	/* No file of that name was found, try some others names. */
	for (encoding = 1; encoding < ENCODINGS_KNOWN; encoding++) {
		unsigned char **ext = listext_encoded(encoding);

		while (ext && *ext) {
			safe_strncpy(filenamepos, *ext, maxlen);

			/* We try with some extensions. */
			*fd = open(filename, O_RDONLY | O_NOCTTY);

			if (*fd >= 0)
				/* Ok, found one, use it. */
				return encoding;

			ext++;
		}
	}

	return ENCODING_NONE;
}

/* Reads the file from @stream in chunks of size @readsize. */
/* Returns a connection state. S_OK if all is well. */
static int
read_file(struct stream_encoded *stream, int readsize, struct file_info *info)
{
	/* + 1 is there because of bug in Linux. Read returns -EACCES when
	 * reading 0 bytes to invalid address */
	unsigned char *fragment = mem_alloc(readsize + 1);
	int fragmentlen;
	int readlen;

	if (!fragment)
		return S_OUT_OF_MEM;

	fragmentlen = 0;
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
			return -errno;
		}

		fragmentlen += readlen;

#if 0
		/* This didn't work so well as it should (I had to implement
		 * end of stream handling to bzip2 anyway), so I rather
		 * disabled this. */
		if (readlen < stt.st_size) {
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

	info->fragment = fragment;
	info->fragmentlen = fragmentlen;
	info->head = stracpy("");
	return S_OK;
}

/* FIXME: Many return values aren't checked. And we should split it.
 * --Zas */
void
file_func(struct connection *c)
{
	struct cache_entry *e;
	unsigned char *fragment;
	unsigned char *filename;
	unsigned char *head;
	int fragmentlen;
	DIR *d;
	int filenamelen;
	struct file_info *info;

	if (get_opt_int_tree(&cmdline_options, "anonymous")) {
		abort_conn_with_state(c, S_BAD_URL);
		return;
	}

	info = mem_alloc(sizeof(struct file_info));
	if (!info) {
		abort_conn_with_state(c, S_OUT_OF_MEM);
		return;
	}

	get_filenamepart_from_url(c->url, &filename, &filenamelen);
	if (!filename) {
		mem_free(info);
		abort_conn_with_state(c, S_OUT_OF_MEM);
		return;
	}

	d = opendir(filename);
	if (d) {
		int state;

		if (filename[0] && !dir_sep(filename[filenamelen - 1])) {
			mem_free(filename);
			mem_free(info);
			closedir(d);

			if (get_cache_entry(c->url, &e)) {
				abort_conn_with_state(c, S_OUT_OF_MEM);
				return;
			}

			c->cache = e;

			if (e->redirect) mem_free(e->redirect);
			e->redirect_get = 1;
			e->redirect = straconcat(c->url, "/", NULL);

			goto end;
		}

		state = list_directory(d, filename, info);
		closedir(d);
		mem_free(filename);

		if (state != S_OK) {
			mem_free(info);
			abort_conn_with_state(c, state);
			return;
		}

		fragment = info->fragment;
		fragmentlen = info->fragmentlen;
		head = info->head;
		mem_free(info);
	} else {
		struct stream_encoded *stream;
		struct stat stt;
		enum stream_encoding encoding = ENCODING_NONE;
		int fd = open(filename, O_RDONLY | O_NOCTTY);
		int state;
		int saved_errno = errno;

		if (fd == -1 && get_opt_bool("protocol.file.try_encoding_extensions")) {
			encoding = try_encoding_extensions(filename, &fd);
		} else if (fd != -1) {
			encoding = guess_encoding(filename);
		}

		mem_free(filename);

		if (fd == -1) {
			mem_free(info);
			abort_conn_with_state(c, -saved_errno);
			return;
		}

		set_bin(fd);
		if (fstat(fd, &stt)) {
			saved_errno = errno;
			mem_free(info);
			close(fd);
			abort_conn_with_state(c, -saved_errno);
			return;
		}

		if (encoding != ENCODING_NONE && !S_ISREG(stt.st_mode)) {
			/* We only want to open regular encoded files. */
			mem_free(info);
			close(fd);
			abort_conn_with_state(c, -saved_errno);
			return;
		}

		if (!S_ISREG(stt.st_mode) &&
		    !get_opt_int("protocol.file.allow_special_files")) {
			close(fd);
			mem_free(info);
			abort_conn_with_state(c, S_FILE_TYPE);
			return;
		}

		stream = open_encoded(fd, encoding);
		state = read_file(stream, stt.st_size, info);

		close_encoded(stream);
		close(fd);

		if (state != S_OK) {
			mem_free(info);
			abort_conn_with_state(c, state);
			return;
		}

		fragment = info->fragment;
		fragmentlen = info->fragmentlen;
		head = info->head;
		mem_free(info);
	}

	if (get_cache_entry(c->url, &e)) {
		mem_free(fragment);
		abort_conn_with_state(c, S_OUT_OF_MEM);
		return;
	}

	if (e->head) mem_free(e->head);
	e->head = head;
	c->cache = e;
	add_fragment(e, 0, fragment, fragmentlen);
	truncate_entry(e, fragmentlen, 1);

	mem_free(fragment);

end:
	c->cache->incomplete = 0;
	abort_conn_with_state(c, S_OK);
}
