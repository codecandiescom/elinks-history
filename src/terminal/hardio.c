/* Low-level terminal-suitable I/O routines */
/* $Id: hardio.c,v 1.4 2003/05/07 20:57:20 zas Exp $ */

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

int
hard_write(int fd, unsigned char *p, int l)
{
	int w = 1;
	int t = 0;

	fprintf(stderr,"[hard_write(fd=%d, p=%p, l=%d)]\n", fd, p, l);

	while (l > 0 && w) {
		w = write(fd, p, l);
		if (w < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
#if 1
			if (w) {
				int hex = 0;
				int i = 0;

				for (; i < w; i++) {
					if (p[i] == ' ') {
						int c = i;

						while (i < w && p[++i] == ' ');

						if (i - c > 2) {
							fprintf(stderr,"[+ %d spaces]\n", i - c - 1);
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
				fflush(stderr);
			}
#endif
		t += w;
		p += w;
		l -= w;
	}
	fputs("\n\n", stderr);
	return t;
}

int
hard_read(int fd, unsigned char *p, int l)
{
	int r = 1;
	int t = 0;

	while (l > 0 && r) {
		r = read(fd, p, l);
		if (r < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
#if 0 /* for debugging purpose */
		{
		 int ww;

		 for (ww = 0; ww < r; ww++)
			 fprintf(stderr, " %02x", (int) p[ww]);
		 fflush(stderr);
		}
#endif
	 	t += r;
		p += r;
		l -= r;
	}

	return t;
}
