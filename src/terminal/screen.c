/* Terminal screen drawing routines. */
/* $Id: screen.c,v 1.9 2003/07/22 00:07:57 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "terminal/hardio.h"
#include "terminal/kbd.h"
#include "terminal/screen.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


/* TODO: We must use termcap/terminfo if available! --pasky */

#define getcompcode(c) ((int)((int)(c)<<1 | ((int)(c)&4)>>2) & 7)

unsigned char frame_dumb[48] =	"   ||||++||++++++--|-+||++--|-+----++++++++     ";
static unsigned char frame_vt100[48] =	"aaaxuuukkuxkjjjkmvwtqnttmlvwtqnvvwwmmllnnjla    ";

/* For UTF8 I/O */
static unsigned char frame_vt100_u[48] = {
	177, 177, 177, 179, 180, 180, 180, 191,
	191, 180, 179, 191, 217, 217, 217, 191,
	192, 193, 194, 195, 196, 197, 195, 195,
	192, 218, 193, 194, 195, 196, 197, 193,
	193, 194, 194, 192, 192, 218, 218, 197,
	197, 217, 218, 177,  32, 32,  32,  32
};

static unsigned char frame_koi[48] = {
	144,145,146,129,135,178,180,167,
	166,181,161,168,174,173,172,131,
	132,137,136,134,128,138,175,176,
	171,165,187,184,177,160,190,185,
	186,182,183,170,169,162,164,189,
	188,133,130,141,140,142,143,139,
};

static unsigned char frame_restrict[48] = {
	0, 0, 0, 0, 0, 179, 186, 186,
	205, 0, 0, 0, 0, 186, 205, 0,
	0, 0, 0, 0, 0, 0, 179, 186,
	0, 0, 0, 0, 0, 0, 0, 205,
	196, 205, 196, 186, 205, 205, 186, 186,
	179, 0, 0, 0, 0, 0, 0, 0,
};


/* In print_char() and redraw_screen(), we must be extremely careful about
 * get_opt() calls, as they are CPU hog here. */

/* TODO: We should provide some generic mechanism for options caching. */
struct rs_opt_cache {
	int type, m11_hack, utf_8_io, colors, charset, restrict_852, cp437, koi8r, trans;
};

/* Time critical section. */
static inline void
print_char(struct terminal *term, struct rs_opt_cache *opt_cache,
	   unsigned char **a, int *l, int p, int *mode, int *attrib)
{
	unsigned ch = term->screen[p];
	unsigned char c = ch & 0xff;
	unsigned char A = ch >> 8 & 0x7f;
	unsigned char B = ch >> 15;

	if (opt_cache->type == TERM_LINUX) {
		if (opt_cache->m11_hack &&
		    !opt_cache->utf_8_io) {
			if (B != *mode) {
				*mode = B;
				if (!*mode) add_to_str(a, l, "\033[10m");
				else add_to_str(a, l, "\033[11m");
			}
		}
		if (opt_cache->restrict_852
		    && B && c >= 176 && c < 224
		    && frame_restrict[c - 176])
			c = frame_restrict[c - 176];
	} else if (opt_cache->type == TERM_VT100
		   && !opt_cache->utf_8_io) {
		if (B != *mode) {
			*mode = B;
			if (!*mode) add_chr_to_str(a, l, '\x0f');
			else add_chr_to_str(a, l, '\x0e');
		}
		if (*mode && c >= 176 && c < 224) c = frame_vt100[c - 176];
	} else if (B && c >= 176 && c < 224) {
		if (opt_cache->type == TERM_VT100)
		   	c = frame_vt100_u[c - 176];
		else if (opt_cache->type == TERM_KOI8)
			c = frame_koi[c - 176];
		else if (opt_cache->type == TERM_DUMB)
		   	c = frame_dumb[c - 176];
	}

	if (!(A & 0100) && (A >> 3) == (A & 7))
		A = (A & 070) | 7 * !(A & 020);

	if (A != *attrib) {
		*attrib = A;
		add_to_str(a, l, "\033[0");
		if (opt_cache->colors) {
			unsigned char m[4];

			m[0] = ';';
		       	m[1] = '3';
			m[2] = (*attrib & 7) + '0';
		       	m[3] = 0;
			add_to_str(a, l, m);
			m[1] = '4';
			m[2] = (*attrib >> 3 & 7) + '0';
			if (!opt_cache->trans || m[2] != '0')
				add_to_str(a, l, m);
		} else if (getcompcode(*attrib & 7) < getcompcode(*attrib >> 3 & 7))
			add_to_str(a, l, ";7");
		if (*attrib & 0100) add_to_str(a, l, ";1");
		add_chr_to_str(a, l, 'm');
	}
	if (c >= ' ' && c != ASCII_DEL /* && c != 155*/) {
		int charset = opt_cache->charset;
		int type = opt_cache->type;

		if (B) {
			int frames_charset = (type == TERM_LINUX ||
					      type == TERM_VT100)
						? opt_cache->cp437
						: type == TERM_KOI8
							? opt_cache->koi8r
							: -1;
			if (frames_charset != -1) charset = frames_charset;
		}
		if (opt_cache->utf_8_io)
			add_to_str(a, l, cp2utf_8(charset, c));
		else
			add_chr_to_str(a, l, c);
	}
	else if (!c || c == 1) add_chr_to_str(a, l, ' ');
	else add_chr_to_str(a, l, '.');
}

/* TODO An optimized add_term_escape_to_string() would be nice. --jonas */

void
redraw_screen(struct terminal *term)
{
	struct rs_opt_cache opt_cache;
	struct string screen;
	register int y = 0;
	register int p = 0;
	int cx = -1;
	int cy = -1;
	int attrib = -1;
	int mode = -1;

	if (!term->dirty || (term->master && is_blocked())) return;

	if (!init_string(&screen)) return;

#if 0
	/* Performance testing utility */
	/* Fill the cache */
	opt_cache.type = 2;
	opt_cache.m11_hack = 1;
	opt_cache.utf_8_io = 0;
	opt_cache.colors = 1;
	opt_cache.charset = 3;
	opt_cache.restrict_852 = 0;
	/* Cache these values as they don't change and
	 * get_cp_index() is pretty CPU-intensive. */
	opt_cache.cp437 = 1;
	opt_cache.koi8r = 1;
#else
	/* Fill the cache */
	opt_cache.type = get_opt_int_tree(term->spec, "type");
	opt_cache.m11_hack = get_opt_bool_tree(term->spec, "m11_hack");
	opt_cache.utf_8_io = get_opt_bool_tree(term->spec, "utf_8_io");
	opt_cache.colors = get_opt_bool_tree(term->spec, "colors");
	opt_cache.charset = get_opt_int_tree(term->spec, "charset");
	opt_cache.restrict_852 = get_opt_bool_tree(term->spec, "restrict_852");
	opt_cache.trans = get_opt_bool_tree(term->spec, "transparency");
	/* Cache these values as they don't change and
	 * get_cp_index() is pretty CPU-intensive. */
	opt_cache.cp437 = get_cp_index("cp437");
	opt_cache.koi8r = get_cp_index("koi8-r");
#endif

	for (; y < term->y; y++) {
		register int x = 0;

		for (; x < term->x; x++, p++) {
#define TSP term->screen[p]
#define TLSP term->last_screen[p]
			if (TSP == TLSP) continue;
			if ((TSP & 0x3800) == (TLSP & 0x3800)) {
				int a = (TSP & 0xff);

				if (a == 0 || a == 1 || a == ' ') {
					a = (TLSP & 0xff);

					if (a == 0 || a == 1 || a == ' ')
						continue;
				}
			}
#undef TSP
#undef TLSP
			if (cx == x && cy == y) {
				print_char(term, &opt_cache, &screen.source, &screen.length,
					   p, &mode, &attrib);
				cx++;
			} else if (cy == y && x - cx < 10) {
				register int i = x - cx;

				for (; i >= 0; i--) {
					print_char(term, &opt_cache, &screen.source, &screen.length,
						   p - i, &mode, &attrib);
					cx++;
				}
			} else {
				add_to_string(&screen, "\033[");
				add_long_to_string(&screen, y + 1);
				add_char_to_string(&screen, ';');
				add_long_to_string(&screen, x + 1);
				add_char_to_string(&screen, 'H');
				cx = x; cy = y;
				print_char(term, &opt_cache, &screen.source, &screen.length,
					   p, &mode, &attrib);
				cx++;
			}
		}
	}

	if (screen.length) {
		if (opt_cache.colors)
			add_to_string(&screen, "\033[37;40m");

		add_to_string(&screen, "\033[0m");

		if (opt_cache.type == TERM_LINUX && opt_cache.m11_hack)
			add_to_string(&screen, "\033[10m");

		if (opt_cache.type == TERM_VT100)
			add_char_to_string(&screen, '\x0f');
	}

	if (screen.length || term->cx != term->lcx || term->cy != term->lcy) {
		term->lcx = term->cx;
		term->lcy = term->cy;
		add_to_string(&screen, "\033[");
		add_long_to_string(&screen, term->cy + 1);
		add_char_to_string(&screen, ';');
		add_long_to_string(&screen, term->cx + 1);
		add_char_to_string(&screen, 'H');
	}

	if (screen.length && term->master) want_draw();
	hard_write(term->fdout, screen.source, screen.length);
	done_string(&screen);
	if (screen.length && term->master) done_draw();

	memcpy(term->last_screen, term->screen, term->x * term->y * sizeof(int));
	term->dirty = 0;
}

void
erase_screen(struct terminal *term)
{
	if (term->master) {
		if (is_blocked()) return;
		want_draw();
	}
	hard_write(term->fdout, "\033[2J\033[1;1H", 10);
	if (term->master) done_draw();
}

void
beep_terminal(struct terminal *term)
{
	hard_write(term->fdout, "\a", 1);
}
