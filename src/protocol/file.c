/* Internal "file" protocol implementation */
/* $Id: file.c,v 1.27 2002/10/13 19:01:16 zas Exp $ */

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

#include "links.h"

#include "config/options.h"
#include "document/cache.h"
#include "lowlevel/sched.h"
#include "protocol/file.h"
#include "util/encoding.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


#ifdef FS_UNIX_RIGHTS
void
setrwx(int m, unsigned char *p)
{
	if(m & S_IRUSR) p[0] = 'r';
	if(m & S_IWUSR) p[1] = 'w';
	if(m & S_IXUSR) p[2] = 'x';
}


void
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


void
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


void
stat_links(unsigned char **p, int *l, struct stat *stp)
{
#ifdef FS_UNIX_HARDLINKS
	unsigned char lnk[64];

	if (!stp) add_to_str(p, l, "    ");
	else {
		sprintf(lnk, "%3d ", (int)stp->st_nlink);
		add_to_str(p, l, lnk);
	}
#endif
}


int last_uid = -1;
char last_user[64];

int last_gid = -1;
char last_group[64];


void
stat_user(unsigned char **p, int *l, struct stat *stp, int g)
{
#ifdef FS_UNIX_USERS
	struct passwd *pwd;
	struct group *grp;
	int id;
	unsigned char *pp;
	int i;

	if (!stp) {
		add_to_str(p, l, "         ");
		return;
	}

	id = !g ? stp->st_uid : stp->st_gid;
	pp = !g ? last_user : last_group;
	if (!g && id == last_uid && last_uid != -1) goto end;
	if (g && id == last_gid && last_gid != -1) goto end;

	if (!g) {
		pwd = getpwuid(id);
		if (!pwd || !pwd->pw_name)
			sprintf(pp, "%d", id);
		else
			sprintf(pp, "%.8s", pwd->pw_name);
		last_uid = id;
	} else {
		grp = getgrgid(id);
		if (!grp || !grp->gr_name)
			sprintf(pp, "%d", id);
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


void
stat_size(unsigned char **p, int *l, struct stat *stp)
{
	unsigned char size[64];

	if (!stp) {
		add_to_str(p, l, "         ");
	} else {
		sprintf(size, "%8ld ", (long)stp->st_size);
		add_to_str(p, l, size);
	}
}


void
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
	wr = strftime(str, 13, fmt, when_local);
#else
	wr = 0;
#endif

	while (wr < 12) str[wr++] = ' ';
	str[12] = '\0';
	add_to_str(p, l, str);
	add_chr_to_str(p, l, ' ');
}

/* TODO: Move this to url.c (redundant with get_filename_from_url() but
 * speeder, imho. 1 should be replaced by POSTCHAR. --Zas */
/* Returns path+filename part (as is) from url as a dynamically allocated
 * string in name and length in namelen or NULL on error. */
void
get_filenamepart_from_url(unsigned char *url, unsigned char **name,
			  int *namelen)
{
	unsigned char *start, *end, *filename;
	int len;

	for (start = url;
	     *start && *start != 1 && *start != ':';
	     start++);

	if (*start != ':' || *++start != '/' || *++start != '/') return;

	start++;

	for (end = start; *end && *end != 1; end++);

	len = end - start;
	filename = mem_alloc(len + 1);

	if (!filename) return;

	if (len) memcpy(filename, start, len);
	filename[len] = '\0';

	*name = filename;
	*namelen = len;
}

static enum stream_encoding
guess_encoding(unsigned char *fname)
{
	int fname_len = strlen(fname);
	unsigned char *fname_end = fname + fname_len;

	/* Case-insensitive?! */

	if (fname_len > 3 && !strcmp(fname_end - 3, ".gz")) {
		return ENCODING_GZIP;
	}

	if (fname_len > 4 && !strcmp(fname_end - 4, ".tgz")) {
		return ENCODING_GZIP;
	}

	if (fname_len > 4 && !strcmp(fname_end - 4, ".bz2")) {
		return ENCODING_BZIP2;
	}

	return ENCODING_NONE;
}


struct dirs {
	unsigned char *s;
	unsigned char *f;
};


int
comp_de(struct dirs *d1, struct dirs *d2)
{
	if (d1->f[0] == '.' && d1->f[1] == '.' && !d1->f[2]) return -1;
	if (d2->f[0] == '.' && d2->f[1] == '.' && !d2->f[2]) return 1;
	if (d1->s[0] == 'd' && d2->s[0] != 'd') return -1;
	if (d1->s[0] != 'd' && d2->s[0] == 'd') return 1;
	return strcmp(d1->f, d2->f);
}

/* FIXME: Many return values aren't checked. And we should split it.
 * --Zas */
void
file_func(struct connection *c)
{
	struct cache_entry *e;
	unsigned char *file, *name, *head;
	int fl;
	DIR *d;
	int h, r;
	struct stat stt;
	int namelen;
	int saved_errno;
	unsigned char dircolor[8];
	int colorize_dir = get_opt_int("document.browse.links.color_dirs");

	if (colorize_dir) {
		color_to_string((struct rgb *) get_opt_ptr("document.colors.dirs"),
				(unsigned char *) &dircolor);
	}

	if (get_opt_int_tree(cmdline_options, "anonymous")) {
		abort_conn_with_state(c, S_BAD_URL);
		return;
	}

	get_filenamepart_from_url(c->url, &name, &namelen);
	if (!name) {
		abort_conn_with_state(c, S_OUT_OF_MEM);
		return;
	}

	h = open(name, O_RDONLY | O_NOCTTY);
	if (h == -1) {
		saved_errno = errno;

		d = opendir(name);
		if (d) goto dir;

		mem_free(name);
		abort_conn_with_state(c, -saved_errno);
		return;
	}

	set_bin(h);
	if (fstat(h, &stt)) {
		saved_errno = errno;
		mem_free(name);
		close(h);
		abort_conn_with_state(c, -saved_errno);
		return;
	}

	if (S_ISDIR(stt.st_mode)) {
		struct dirs *dir;
		int dirl;
		int i;
		struct dirent *de;

		d = opendir(name);


		close(h);

dir:
		dir = DUMMY;
		dirl = 0;

		if (name[0] && !dir_sep(name[namelen - 1])) {
			if (get_cache_entry(c->url, &e)) {
				mem_free(name);
				closedir(d);
				abort_conn_with_state(c, S_OUT_OF_MEM);
				return;
			}
			c->cache = e;

			if (e->redirect) mem_free(e->redirect);
			e->redirect = stracpy(c->url);
			e->redirect_get = 1;
			add_to_strn(&e->redirect, "/");
			mem_free(name);
			closedir(d);

			goto end;
		}

		last_uid = -1;
		last_gid = -1;
		file = init_str();
		fl = 0;

		add_to_str(&file, &fl, "<html>\n<head><title>");
		add_htmlesc_str(&file, &fl, name, strlen(name));
		add_to_str(&file, &fl, "</title></head>\n<body>\n<h2>Directory ");
		add_htmlesc_str(&file, &fl, name, strlen(name));
		add_to_str(&file, &fl, "</h2>\n<pre>");

		while ((de = readdir(d))) {
			struct stat stt, *stp;
			unsigned char **p;
			int l;
			struct dirs *nd;
			unsigned char *n;

			if (!strcmp(de->d_name, ".")) continue;

			nd = mem_realloc(dir, (dirl + 1) * sizeof(struct dirs));
			if (!nd) continue;

			dir = nd;
			dir[dirl].f = stracpy(de->d_name);

			*(p = &dir[dirl++].s) = init_str();

			l = 0;
			n = stracpy(name);
			add_to_strn(&n, de->d_name);
#ifdef FS_UNIX_SOFTLINKS
			if (lstat(n, &stt))
#else
			if (stat(n, &stt))
#endif
				stp = NULL;
			else
				stp = &stt;

			mem_free(n);
			stat_mode(p, &l, stp);
			stat_links(p, &l, stp);
			stat_user(p, &l, stp, 0);
			stat_user(p, &l, stp, 1);
			stat_size(p, &l, stp);
			stat_date(p, &l, stp);
		}

		closedir(d);

		if (dirl) qsort(dir, dirl, sizeof(struct dirs),
				(int(*)(const void *, const void *))comp_de);

		for (i = 0; i < dirl; i++) {
			unsigned char *lnk = NULL;

#ifdef FS_UNIX_SOFTLINKS
			if (dir[i].s[0] == 'l') {

				unsigned char *buf = NULL;
				int size = 0;
				int r = -1;
				unsigned char *n = init_str();
				int nl = 0;

				add_to_str(&n, &nl, name);
				add_htmlesc_str(&n, &nl,
						dir[i].f, strlen(dir[i].f));
				do {
					if (buf) mem_free(buf);
					size += ALLOC_GR;
					buf = mem_alloc(size);
					if (!buf) break;
					r = readlink(n, buf, size);
				} while (r == size);

				mem_free(n);
				if (buf) {
					if (r != -1) {
						buf[r] = '\0';

						lnk = buf;
					} else {
						mem_free(buf);
					}
				}
			}
#endif
			/* add_to_str(&file, &fl, "   "); */
			add_htmlesc_str(&file, &fl,
					dir[i].s, strlen(dir[i].s));
			add_to_str(&file, &fl, "<a href=\"");
			add_htmlesc_str(&file, &fl,
					dir[i].f, strlen(dir[i].f));
			if (dir[i].s[0] == 'd') {
				add_to_str(&file, &fl, "/");
			} else if (lnk) {
				struct stat st;
				unsigned char *n = init_str();
				int nl = 0;

				add_to_str(&n, &nl, name);
				add_htmlesc_str(&n, &nl,
						dir[i].f, strlen(dir[i].f));
				if (!stat(n, &st))
					if (S_ISDIR(st.st_mode))
						add_to_str(&file, &fl, "/");
				mem_free(n);
			}
			add_to_str(&file, &fl, "\">");

			if (dir[i].s[0] == 'd' && colorize_dir) {
				/* The <b> is here for the case when we've
				 * use_document_colors off. */
				add_to_str(&file, &fl, "<font color=\"");
				add_to_str(&file, &fl, dircolor);
				add_to_str(&file, &fl, "\"><b>");
			}

			add_htmlesc_str(&file, &fl,
					dir[i].f, strlen(dir[i].f));

			if (dir[i].s[0] == 'd' && colorize_dir) {
				add_to_str(&file, &fl, "</b></font>");
			}

			add_to_str(&file, &fl, "</a>");
			if (lnk) {
				add_to_str(&file, &fl, " -> ");
				add_htmlesc_str(&file, &fl, lnk, strlen(lnk));
				mem_free(lnk);
			}

			add_to_str(&file, &fl, "\n");
		}

		mem_free(name);
		for (i = 0; i < dirl; i++) {
			mem_free(dir[i].s);
		       	mem_free(dir[i].f);
		}
		mem_free(dir);

		add_to_str(&file, &fl, "</pre>\n<hr>\n</body>\n</html>\n");
		head = stracpy("\r\nContent-Type: text/html\r\n");

	} else if (!S_ISREG(stt.st_mode)) {
		const int bufsize = 4096;
		int offset = 0;

		mem_free(name);

		if (!get_opt_int("protocol.file.allow_special_files")) {
			close(h);
			abort_conn_with_state(c, S_FILE_TYPE);
			return;
		}

		file = mem_alloc(bufsize + 1);
		if (!file) {
			close(h);
			abort_conn_with_state(c, S_OUT_OF_MEM);
			return;
		}

		while ((r = read(h, file + offset, bufsize)) > 0) {
			offset += r;

			file = mem_realloc(file, offset + bufsize + 1);
			if (!file) {
				close(h);
				abort_conn_with_state(c, S_OUT_OF_MEM);
				return;
			}
		}

		fl = offset;
		file[fl] = '\0'; /* NULL-terminate just in case */

		close(h);

		head = stracpy("");

	} else {
		struct stream_encoded *stream;
		enum stream_encoding encoding = guess_encoding(name);

		mem_free(name);

		/* We read with granularity of stt.st_size - this does best
		 * job for uncompressed files, and doesn't hurt for compressed
		 * ones anyway - very large files usually tend to inflate fast
		 * anyway. At least I hope ;). --pasky */

		/* + 1 is there because of bug in Linux. Read returns -EACCES
		 * when reading 0 bytes to invalid address */

		file = mem_alloc(stt.st_size + 1);
		if (!file) {
			close(h);
			abort_conn_with_state(c, S_OUT_OF_MEM);
			return;
		}

		stream = open_encoded(h, encoding);
		fl = 0;
		while ((r = read_encoded(stream, file + fl, stt.st_size))) {
			if (r < 0) {
				/* FIXME: We should get the correct error
				 * value. But it's I/O error in 90% of cases
				 * anyway.. ;) --pasky */
				saved_errno = errno;
				mem_free(file);
				close_encoded(stream);
				abort_conn_with_state(c, -saved_errno);
				return;
			}

			fl += r;

#if 0
			/* This didn't work so well as it should (I had to
			 * implement end of stream handling to bzip2 anyway),
			 * so I rather disabled this. */
			if (r < stt.st_size) {
				/* This is much safer. It should always mean
				 * that we already read everything possible,
				 * and it permits us more elegant of handling
				 * end of file with bzip2. */
				break;
			}
#endif

			file = mem_realloc(file, fl + stt.st_size);
			if (!file) {
				close_encoded(stream);
				abort_conn_with_state(c, S_OUT_OF_MEM);
				return;
			}
		}
		close_encoded(stream);

		head = stracpy("");
	}

	if (get_cache_entry(c->url, &e)) {
		mem_free(file);
		abort_conn_with_state(c, S_OUT_OF_MEM);
		return;
	}

	if (e->head) mem_free(e->head);
	e->head = head;
	c->cache = e;
	add_fragment(e, 0, file, fl);
	truncate_entry(e, fl, 1);

	mem_free(file);

end:
	c->cache->incomplete = 0;
	abort_conn_with_state(c, S_OK);
}
