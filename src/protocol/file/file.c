/* Internal "file" protocol implementation */
/* $Id: file.c,v 1.43 2003/06/04 10:12:11 zas Exp $ */

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
#include "util/encoding.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


#ifdef FS_UNIX_RIGHTS
static inline void
setrwx(int m, unsigned char *p)
{
	if(m & S_IRUSR) p[0] = 'r';
	if(m & S_IWUSR) p[1] = 'w';
	if(m & S_IXUSR) p[2] = 'x';
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

static enum stream_encoding
guess_encoding(unsigned char *fname)
{
	int fname_len = strlen(fname);
	unsigned char *fname_end = fname + fname_len;
	unsigned char **ext;
	int enc;

	for (enc = 1; enc < NB_KNOWN_ENCODING; enc++) {
		ext = listext_encoded(enc);
		while (ext && *ext) {
			int len = strlen(*ext);

			if (fname_len > len
			    && !strcmp(fname_end - len, *ext))
				return enc;
			ext++;
		}
	}

	return ENCODING_NONE;
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
	unsigned char **ext;
	int enc;
	enum stream_encoding encoding = ENCODING_NONE;

	if (colorize_dir) {
		color_to_string((struct rgb *) get_opt_ptr("document.colors.dirs"),
				(unsigned char *) &dircolor);
	}

	if (get_opt_int_tree(&cmdline_options, "anonymous")) {
		abort_conn_with_state(c, S_BAD_URL);
		return;
	}

	get_filenamepart_from_url(c->url, &name, &namelen);
	if (!name) {
		abort_conn_with_state(c, S_OUT_OF_MEM);
		return;
	}

	/* First, we try with name as is. */
	h = open(name, O_RDONLY | O_NOCTTY);
	saved_errno = errno;
	if (h == -1 && get_opt_bool("protocol.file.try_encoding_extensions")) {
		/* No file of that name was found, try some others names. */
		for (enc = 1; enc < NB_KNOWN_ENCODING; enc++) {
			ext = listext_encoded(enc);
			while (ext && *ext) {
				unsigned char *tname = init_str();
				int tname_len = 0;

				if (!tname) {
					mem_free(name);
					abort_conn_with_state(c, S_OUT_OF_MEM);
					return;
				}

				add_to_str(&tname, &tname_len, name);
				add_to_str(&tname, &tname_len, *ext);

				/* We try with some extensions. */
				h = open(tname, O_RDONLY | O_NOCTTY);
				if (h >= 0) {
					/* Ok, found one, use it. */
					mem_free(name);
					name = tname;
					namelen = strlen(tname);
					encoding = enc;
					enc = NB_KNOWN_ENCODING;
					break;
				}
				mem_free(tname);
				ext++;
			}
		}
	}

	if (h == -1) {
		d = opendir(name);
		if (d) goto dir;

		mem_free(name);
		abort_conn_with_state(c, -saved_errno);
		return;
	}

	set_bin(h);
	if (fstat(h, &stt)) {
		saved_errno = errno;
		close(h);
		mem_free(name);
		abort_conn_with_state(c, -saved_errno);
		return;
	}

	if (encoding != ENCODING_NONE && !S_ISREG(stt.st_mode)) {
		/* We only want to open regular encoded files. */
		close(h);
		mem_free(name);
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
		dir = NULL;
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
			e->redirect_get = 1;
			e->redirect = stracpy(c->url);
			if (e->redirect) add_to_strn(&e->redirect, "/");
			mem_free(name);
			closedir(d);

			goto end;
		}

		last_uid = -1;
		last_gid = -1;
		file = init_str();
		fl = 0;

		if (!file) {
			close(h);
			abort_conn_with_state(c, S_OUT_OF_MEM);
			return;
		}

		add_to_str(&file, &fl, "<html>\n<head><title>");
		add_htmlesc_str(&file, &fl, name, strlen(name));
		add_to_str(&file, &fl, "</title></head>\n<body>\n<h2>Directory ");
		{
			unsigned char *pslash, *slash = name - 1;

			while (pslash = ++slash, slash = strchr(slash, '/')) {
				if (slash == name) {
					add_chr_to_str(&file, &fl, '/');
					continue;
				}

				slash[0] = 0;
				add_to_str(&file, &fl, "<a href=\"");
				/* FIXME: htmlesc? At least we should escape quotes. --pasky */
				add_to_str(&file, &fl, name);
				add_chr_to_str(&file, &fl, '/');
				add_to_str(&file, &fl, "\">");
				add_htmlesc_str(&file, &fl, pslash, strlen(pslash));
				add_to_str(&file, &fl, "</a>");
				add_chr_to_str(&file, &fl, '/');
				slash[0] = '/';
			}
		}
		add_to_str(&file, &fl, "</h2>\n<pre>");

		while ((de = readdir(d))) {
			struct stat st, *stp;
			unsigned char **p;
			int l;
			struct dirs *nd;
			unsigned char *n;

			if (!strcmp(de->d_name, ".")) continue;

			nd = mem_realloc(dir, (dirl + 1) * sizeof(struct dirs));
			if (!nd) continue;

			dir = nd;
			dir[dirl].f = stracpy(de->d_name);
			if (!dir[dirl].f) continue;

			p = &dir[dirl++].s;
			*p = init_str();
			if (!*p) continue;

			l = 0;
			n = stracpy(name);
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

		closedir(d);

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
				add_to_str(&n, &nl, name);
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
			/* add_to_str(&file, &fl, "   "); */
			add_htmlesc_str(&file, &fl,
					dir[i].s, strlen(dir[i].s));
			add_to_str(&file, &fl, "<a href=\"");
			add_htmlesc_str(&file, &fl,
					dir[i].f, strlen(dir[i].f));
			if (dir[i].s[0] == 'd') {
				add_chr_to_str(&file, &fl, '/');
			} else if (lnk) {
				struct stat st;
				unsigned char *n = init_str();
				int nl = 0;

				if (n) {
					add_to_str(&n, &nl, name);
					add_htmlesc_str(&n, &nl,
							dir[i].f, strlen(dir[i].f));
					if (!stat(n, &st))
						if (S_ISDIR(st.st_mode))
							add_chr_to_str(&file, &fl, '/');
					mem_free(n);
				}
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

			add_chr_to_str(&file, &fl, '\n');
		}

		mem_free(name);
		for (i = 0; i < dirl; i++) {
			if (dir[i].s) mem_free(dir[i].s);
		       	if (dir[i].f) mem_free(dir[i].f);
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

		if (encoding == ENCODING_NONE)
			encoding = guess_encoding(name);

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
