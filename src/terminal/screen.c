/* Terminal screen drawing routines. */
/* $Id: screen.c,v 1.49 2003/08/29 11:31:50 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "terminal/draw.h"
#include "terminal/hardio.h"
#include "terminal/kbd.h"
#include "terminal/screen.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


/* TODO: We must use termcap/terminfo if available! --pasky */

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
	144, 145, 146, 129, 135, 178, 180, 167,
	166, 181, 161, 168, 174, 173, 172, 131,
	132, 137, 136, 134, 128, 138, 175, 176,
	171, 165, 187, 184, 177, 160, 190, 185,
	186, 182, 183, 170, 169, 162, 164, 189,
	188, 133, 130, 141, 140, 142, 143, 139,
};

/* Most of this table is just 176 + <index in table>. */
static unsigned char frame_restrict[48] = {
	176, 177, 178, 179, 180, 179, 186, 186,
	205, 185, 186, 187, 188, 186, 205, 191,
	192, 193, 194, 195, 196, 197, 179, 186,
	200, 201, 202, 203, 204, 205, 206, 205,
	196, 205, 196, 186, 205, 205, 186, 186,
	179, 217, 218, 219, 220, 221, 222, 223,
};


/* In print_char() and redraw_screen(), we must be extremely careful about
 * get_opt() calls, as they are CPU hog here. */

/* TODO: We should provide some generic mechanism for options caching. */
struct rs_opt_cache {
	int type;
	int charset;
	int cp437;
	int koi8r;
	unsigned int m11_hack:1;
	unsigned int utf_8_io:1;
	unsigned int colors:1;
	unsigned int restrict_852:1;
	unsigned int trans:1;
};

struct screen_state {
	unsigned char *frame;
	unsigned char color;
	unsigned char border;
	unsigned char underline;
};

/* Time critical section. */
static inline void
print_char(struct string *screen, struct rs_opt_cache *opt_cache,
	   struct screen_char *ch, struct screen_state *state)
{
	unsigned char c = ch->data;
	unsigned char color = ch->color;
	unsigned char underline = (ch->attr & SCREEN_ATTR_UNDERLINE);
	unsigned char border = (ch->attr & SCREEN_ATTR_FRAME);

	if (opt_cache->type == TERM_LINUX) {
		if (opt_cache->m11_hack && !opt_cache->utf_8_io) {
			if (border != state->border) {
				state->border = border;

				if (!border) {
					add_bytes_to_string(screen, "\033[10m", 5);
				} else {
					add_bytes_to_string(screen, "\033[11m", 5);
				}
			}
		}

		if (opt_cache->restrict_852 && border && c >= 176 && c < 224) {
			c = state->frame[c - 176];
		}

	} else {
		if (state->frame == frame_vt100 && border != state->border) {
			state->border = border;

			if (!border) {
				add_char_to_string(screen, '\x0f');
			} else {
				add_char_to_string(screen, '\x0e');
			}
		}

		if (border && c >= 176 && c < 224) {
			c = state->frame[c - 176];
		}
	}

	if (color != state->color) {
		unsigned char code[11];
		int length;

		state->color = color;

		code[0] = '\033';
		code[1] = '[';
		code[2] = '0';

		if (opt_cache->colors) {
			code[3] = ';';
			code[4] = '3';
			code[5] = (color & 7) + '0';

			code[8] = (color >> 3 & 7) + '0';
			if (!opt_cache->trans || code[8] != '0') {
				code[6] = ';';
				code[7] = '4';
				length = 9;
			} else {
				length = 6;
			}

#define getcompcode(c) ((int)((int)(c)<<1 | ((int)(c)&4)>>2) & 7)

		} else if (getcompcode(color & 7) < getcompcode(color >> 3 & 7)) {
			code[3] = ';';
			code[4] = '7';
			length = 5;
		} else {
			length = 3;
		}

		if (color & 0100) {
			code[length++] = ';';
			code[length++] = '1';
		}

		code[length++] = 'm';

		add_bytes_to_string(screen, code, length);
	}

	if (opt_cache->type == TERM_VT100) {
		if (underline != state->underline) {
			state->underline = underline;

			if (underline) {
				add_bytes_to_string(screen, "\033[4m", 4);
			} else {
				add_bytes_to_string(screen, "\033[24m", 5);
			}
		}
	}

	if (c >= ' ' && c != ASCII_DEL /* && c != 155*/) {
		if (opt_cache->utf_8_io) {
			int charset = opt_cache->charset;

			if (border) {
				switch (opt_cache->type) {
					case TERM_LINUX:
					case TERM_VT100:
						charset = opt_cache->cp437;
						break;
					case TERM_KOI8:
						charset = opt_cache->koi8r;
						break;
					default:
						break;
				}
			}

			add_to_string(screen, cp2utf_8(charset, c));
		} else {
			add_char_to_string(screen, c);
		}
	} else if (c <= 1) {
		add_char_to_string(screen, ' ');
	} else {
		add_char_to_string(screen, '.');
	}
}


/* Adds the term code for positioning the cursor at @x and @y to @string.
 * The template term code is: "\033[<@y>;<@x>H" */
static inline struct string *
add_cursor_move_to_string(struct string *screen, int y, int x)
{
	/* 28 chars for both of the @y and @x numbers should be enough. */
	unsigned char code[32];
	int length = 2;
	int ret;

	code[0] = '\033';
	code[1] = '[';

	ret = longcat(code, &length, y, 30, 0);
	/* Make sure theres atleast room for ';' and `some' number ;) */
	if (ret < 0 || length > 30) return NULL;

	code[length++] = ';';

	ret = longcat(code, &length, x, sizeof(code) - length, 0);
	if (ret < 0 || length > 31) return NULL;

	code[length++] = 'H';

	return add_bytes_to_string(screen, code, length);
}

#if 0
/* Performance testing utility */
#define fill_option_cache(c, t) do { \
		(c).type	 = 2; \
		(c).m11_hack	 = 1; \
		(c).utf_8_io	 = 0; \
		(c).colors	 = 1; \
		(c).charset	 = 3; \
		(c).restrict_852 = 0; \
		(c).cp437	 = 1; \
		(c).koi8r	 = 1; \
	} while (0)
#else
/* Fill the cache */
#define fill_option_cache(c, t) do { \
		(c).type	 = get_opt_int_tree((t)->spec,	"type"); \
		(c).m11_hack	 = get_opt_bool_tree((t)->spec,	"m11_hack"); \
		(c).utf_8_io	 = get_opt_bool_tree((t)->spec,	"utf_8_io"); \
		(c).colors	 = get_opt_bool_tree((t)->spec,	"colors"); \
		(c).charset	 = get_opt_int_tree((t)->spec,	"charset"); \
		(c).restrict_852 = get_opt_bool_tree((t)->spec,	"restrict_852"); \
		(c).trans	 = get_opt_bool_tree((t)->spec,	"transparency"); \
		\
		/* Cache these values as they don't change and
		 * get_cp_index() is pretty CPU-intensive. */ \
		(c).cp437	 = get_cp_index("cp437"); \
		(c).koi8r	 = get_cp_index("koi8-r"); \
	} while (0)
#endif

static inline unsigned char *
get_frame_table(struct rs_opt_cache *opt_cache)
{
	switch (opt_cache->type) {
		case TERM_LINUX:
			return frame_restrict;

		case TERM_VT100:
			return (opt_cache->utf_8_io)
				? frame_vt100_u : frame_vt100;

		case TERM_KOI8:
			return frame_koi;

		default:
			return frame_dumb;
	}
}

/* Updating of the terminal screen is done by checking what needs to be updated
 * using the last screen. */
void
redraw_screen(struct terminal *term)
{
	struct rs_opt_cache opt_cache;
	struct string image;
	register int y = 0;
	int prev_y = -1;
	struct screen_state state = { NULL, 0xFF, 0xFF, 0xFF };
	struct terminal_screen *screen = term->screen;
 	register struct screen_char *current;
 	register struct screen_char *pos;
 	register struct screen_char *prev_pos;

	if (!screen
	    || !screen->dirty
	    || (term->master && is_blocked())
	    || !init_string(&image)) return;

 	fill_option_cache(opt_cache, term);
	state.frame = get_frame_table(&opt_cache);

	current = screen->last_image;
 	pos = screen->image;
 	prev_pos = NULL;

 	for (; y < term->y; y++) {
 		register int x = 0;

 		for (; x < term->x; x++, current++, pos++) {

			/* No update for exact match. */
 			if (pos->data == current->data
 			    && pos->color == current->color
 			    && pos->attr == current->attr)
				continue;

			/* Else if the color match and the data is ``space''. */
 			if (pos->color == current->color
			    && (pos->data <= 1 || pos->data == ' ')
			    && (current->data <= 1 || current->data == ' '))
				continue;

			/* Move the cursor when @prev_pos is more than 10 chars
			 * away. */
 			if (prev_y != y || prev_pos + 10 <= pos) {
 				add_cursor_move_to_string(&image, y + 1, x + 1);
 				prev_pos = pos;
				prev_y = y;
			}

			for (; prev_pos <= pos ; prev_pos++)
				print_char(&image, &opt_cache, prev_pos, &state);
		}
	}

	if (image.length) {
		if (opt_cache.colors)
			add_bytes_to_string(&image, "\033[37;40m", 8);

		add_bytes_to_string(&image, "\033[0m", 4);

		if (opt_cache.type == TERM_LINUX && opt_cache.m11_hack)
			add_bytes_to_string(&image, "\033[10m", 5);

		if (opt_cache.type == TERM_VT100)
			add_char_to_string(&image, '\x0f');
	}

	if (image.length
	    || screen->cx != screen->lcx
	    || screen->cy != screen->lcy) {
		screen->lcx = screen->cx;
		screen->lcy = screen->cy;

		add_cursor_move_to_string(&image, screen->cy + 1, screen->cx + 1);
	}

	if (image.length) {
		if (term->master) want_draw();
		hard_write(term->fdout, image.source, image.length);
		if (term->master) done_draw();
	}

	done_string(&image);

	memcpy(screen->last_image, screen->image, term->x * term->y * sizeof(struct screen_char));
	screen->dirty = 0;
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

struct terminal_screen *
init_screen(void)
{
	struct terminal_screen *screen;

	screen = mem_calloc(1, sizeof(struct terminal_screen));
	if (!screen) return NULL;

	screen->lcx = -1;
	screen->lcy = -1;
	screen->dirty = 1;

	return screen;
}

/* The two images are allocated in one chunk. */
/* TODO: It seems allocation failure here is fatal. We should do something! */
void
resize_screen(struct terminal *term, int x, int y)
{
	int size = x * y;
	int bsize = size * sizeof(struct screen_char);
	struct terminal_screen *screen = term->screen;
	struct screen_char *image;

	assert(screen);

	image = mem_realloc(screen->image, bsize<<1);
	if (!image) return;

	screen->image = image;
	screen->last_image = image + size;

	memset(screen->image, 0, bsize);
	memset(screen->last_image, 0xFF, bsize);

	term->x = x;
	term->y = y;
	screen->dirty = 1;
}

void
done_screen(struct terminal_screen *screen)
{
	if (screen->image) mem_free(screen->image);
	mem_free(screen);
}
