/* Low-level terminal-suitable I/O routines */
/* $Id: hardio.c,v 1.6 2003/05/08 01:23:15 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "terminal/hardio.h"

/* Define it to have data written to stderr */
/* #define HW_DEBUG */

#undef debug_open
#undef debug_flush
#undef debug_write

#ifdef HW_DEBUG
static void
hw_debug_open(unsigned char *name, int fd, unsigned char *p, int l)
{
	fprintf(stderr, "[%s (fd=%d, p=%p, l=%d)]\n",name, fd, p, l);
}

static void
hw_debug_flush() {
	fputs("\n\n", stderr);
	fflush(stderr);
}

static void
hw_debug_write(unsigned char *p, int w)
{
	int hex = 0;
	int i = 0;

	if (!w) return;

	for (; i < w; i++) {
		if (p[i] == ' ') {
			int c = i;

			while (i < w && p[++i] == ' ');

			if (i - c - 1 > 1) {
				fprintf(stderr, "[+ %d spaces]\n", i - c - 1);
				if (i == w) break;
				c = 0;
				continue;
			}
			c = 0;
			i--;
		}

		if (p[i] >= ' ' && p[i] < 127 && p[i] != '|') {
			if (hex) {
				fputc('|', stderr);
				hex = 0;
			}
			fputc(p[i], stderr);
		} else {
			if (!hex) {
				fputc('|', stderr);
				hex = 1;
			}
			fprintf(stderr,"%02x", p[i]);
		}
	}
}
#define debug_open(n, fd, p, l) hw_debug_open(n, fd, p, l)
#define debug_flush() hw_debug_flush()
#define debug_write(p, l) hw_debug_write(p, l)
#else
#define debug_open(n, fd, p, l)
#define debug_flush()
#define debug_write(p, l)
#endif


int
hard_write(int fd, unsigned char *p, int l)
{
	int w = 1;
	int t = 0;

	debug_open("hard_write", fd, p, l);

	while (l > 0 && w) {
		w = write(fd, p, l);
		if (w < 0) {
			if (errno == EINTR) continue;
			return -1;
		}

		debug_write(p, w);

		t += w;
		p += w;
		l -= w;
	}

	debug_flush();

	return t;
}

int
hard_read(int fd, unsigned char *p, int l)
{
	int r = 1;
	int t = 0;

	debug_open("hard_read", fd, p, l);

	while (l > 0 && r) {
		r = read(fd, p, l);
		if (r < 0) {
			if (errno == EINTR) continue;
			return -1;
		}

		debug_write(p, r);

		t += r;
		p += r;
		l -= r;
	}

	debug_flush();

	return t;
}


