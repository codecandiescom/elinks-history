/* Terminal screen drawing routines. */
/* $Id: screen.c,v 1.70 2003/09/03 22:34:59 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "terminal/color.h"
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

struct frame_seq {
	unsigned char *src;
	int len;
};

static struct frame_seq m11_hack_frame_seqs[] = {
	/* end border: */	{ "\033[10m", 5 },
	/* begin border: */	{ "\033[11m", 5 },
};

static struct frame_seq vt100_frame_seqs[] = {
	/* end border: */	{ "\x0f", 1 },
	/* begin border: */	{ "\x0e", 1 },
};

/* Used in print_char() and redraw_screen() to reduce the logic. */
/* TODO: termcap/terminfo can maybe gradually be introduced via this
 *	 structure. We'll see. --jonas */
struct screen_driver {
	LIST_HEAD(struct screen_driver);

	/* The terminal._template_.type. Together with the @name member the
	 * uniquely identify the screen_driver. */
	enum term_mode_type type;

	/* Charsets when doing UTF8 I/O. */
	/* [0] is the common charset and [1] is the frame charset.
	 * Test wether to use UTF8 I/O using the use_utf8_io() macro. */
	int charsets[2];

	/* The frame translation table. May be NULL. */
	unsigned char *frame;

	/* The frame mode setup and teardown sequences. May be NULL. */
	struct frame_seq *frame_seqs;

	/* These are directly derived from the terminal options. */
	unsigned int colors:1;
	unsigned int trans:1;
	unsigned int underline:1;

	/* The terminal._template_ name. */
	unsigned char name[1]; /* XXX: Keep last! */
};

static struct screen_driver dumb_screen_driver = {
				NULL_LIST_HEAD,
	/* type: */		TERM_DUMB,
	/* charsets: */		{ -1, -1 },	/* No UTF8 I/O */
	/* frame: */		frame_dumb,
	/* frame_seqs: */	NULL,
	/* colors: */		1,
	/* trans: */		1,
	/* underline: */	1,
};

static struct screen_driver vt100_screen_driver = {
				NULL_LIST_HEAD,
	/* type: */		TERM_VT100,
	/* charsets: */		{ -1, -1 },	/* No UTF8 I/O */
	/* frame: */		frame_vt100,	/* No UTF8 I/O */
	/* frame_seqs: */	vt100_frame_seqs, /* No UTF8 I/O */
	/* colors: */		1,
	/* trans: */		1,
	/* underline: */	1,
};

static struct screen_driver linux_screen_driver = {
				NULL_LIST_HEAD,
	/* type: */		TERM_LINUX,
	/* charsets: */		{ -1, -1 },	/* No UTF8 I/O */
	/* frame: */		NULL,		/* No restrict_852 */
	/* frame_seqs: */	NULL,		/* No m11_hack */
	/* colors: */		1,
	/* trans: */		1,
	/* underline: */	1,
};

static struct screen_driver koi8_screen_driver = {
				NULL_LIST_HEAD,
	/* type: */		TERM_KOI8,
	/* charsets: */		{ -1, -1 },	/* No UTF8 I/O */
	/* frame: */		frame_koi,
	/* frame_seqs: */	NULL,
	/* colors: */		1,
	/* trans: */		1,
	/* underline: */	1,
};

/* XXX: Keep in sync with enum term_mode_type. */
static struct screen_driver *screen_drivers[] = {
	/* TERM_DUMB: */	&dumb_screen_driver,
	/* TERM_VT100: */	&vt100_screen_driver,
	/* TERM_LINUX: */	&linux_screen_driver,
	/* TERM_KOI8: */	&koi8_screen_driver,
};

static INIT_LIST_HEAD(active_screen_drivers);

static void
update_screen_driver(struct screen_driver *driver, struct option *term_spec)
{
	int utf8_io = get_opt_bool_tree(term_spec, "utf_8_io");

	driver->colors = get_opt_bool_tree(term_spec, "colors");
	driver->trans = get_opt_bool_tree(term_spec, "transparency");
	driver->underline = get_opt_bool_tree(term_spec, "underline");

	if (utf8_io) {
		driver->charsets[0] = get_opt_int_tree(term_spec, "charset");
	}

	if (driver->type == TERM_LINUX) {
		if (get_opt_bool_tree(term_spec, "restrict_852")) {
			driver->frame = frame_restrict;
		}

		if (utf8_io) {
			driver->charsets[1] = get_cp_index("cp437");

		} else if (get_opt_bool_tree(term_spec, "m11_hack")) {
			driver->frame_seqs = m11_hack_frame_seqs;
		}

	} else if (driver->type == TERM_VT100) {
		if (utf8_io) {
			driver->frame = frame_vt100_u;
			driver->charsets[1] = get_cp_index("cp437");
		}

	} else if (driver->type == TERM_KOI8) {
		if (utf8_io) {
			driver->charsets[1] = get_cp_index("koi8-r");
		}
	} else {
		if (utf8_io) {
			driver->charsets[1] = driver->charsets[0];
		}
	}
}

static int
screen_driver_change_hook(struct session *ses, struct option *term_spec,
			  struct option *changed)
{
	enum term_mode_type type = get_opt_int_tree(term_spec, "type");
	struct screen_driver *driver;
	unsigned char *name = term_spec->name;
	int len = strlen(name);

	foreach (driver, active_screen_drivers)
		if (driver->type == type && !memcmp(driver->name, name, len)) {
			update_screen_driver(driver, term_spec);
			break;
		}

	return 0;
}

static inline struct screen_driver *
add_screen_driver(enum term_mode_type type, struct terminal *term, int env_len)
{
	struct screen_driver *driver;

	driver = mem_alloc(sizeof(struct screen_driver) + env_len + 1);
	if (!driver) return NULL;

	memcpy(driver, screen_drivers[type], sizeof(struct screen_driver));
	memcpy(driver->name, term->term, env_len + 1);

	add_to_list(active_screen_drivers, driver);

	update_screen_driver(driver, term->spec);

	term->spec->change_hook = screen_driver_change_hook;

	return driver;
}

static inline struct screen_driver *
get_screen_driver(struct terminal *term)
{
	enum term_mode_type type = get_opt_int_tree(term->spec, "type");
	unsigned char *name = term->spec->name;
	int len = strlen(name); 
	struct screen_driver *driver;

	/* TODO: LRU? ;) */
	foreach (driver, active_screen_drivers) {
		if (driver->type == type && !memcmp(driver->name, name, len)) {
			return driver;
		}
	}

	return add_screen_driver(type, term, len);
}

void
done_screen_drivers(void)
{
	free_list(active_screen_drivers);
}


struct screen_state {
	unsigned char color;
	unsigned char border;
	unsigned char underline;
};

/* When determining wether to use negative image we make the most significant
 * be least significant. */
#define CMPCODE(c) (((c) << 1 | (c) >> 2) & TERM_COLOR_MASK)
#define use_negative_image(c) \
	(CMPCODE(TERM_COLOR_FOREGROUND(c)) < CMPCODE(TERM_COLOR_BACKGROUND(c)))

#define use_utf8_io(driver) ((driver)->charsets[0] != -1)

/* Time critical section. */
static inline void
print_char(struct string *screen, struct screen_driver *driver,
	   struct screen_char *ch, struct screen_state *state)
{
	unsigned char c = ch->data;
	unsigned char color = ch->color;
	unsigned char border = (ch->attr & SCREEN_ATTR_FRAME);
	unsigned char underline = (ch->attr & SCREEN_ATTR_UNDERLINE);

	if (border != state->border && driver->frame_seqs) {
		register struct frame_seq *seq = &driver->frame_seqs[!!border];

		state->border = border;
		add_bytes_to_string(screen, seq->src, seq->len);
	}

	/* This is optimized for the (common) case that underlines are rare. */
	if (underline != state->underline) {
		/* Underline is optional which makes everything a bit more
		 * complicated. */
		if (!driver->underline) {
			/* If underlines should _not_ be drawn color
			 * enhancements have to be applied _before_ adding the
			 * color below and the state should not be touch
			 * because we want to apply enhancements for each
			 * underlined char. */
			if (underline) {
				color |= SCREEN_ATTR_BOLD;
				color ^= 0x04;

				/* Mark that underline has been handled so it
				 * is not added when adding the color below. */
				underline = 0;
			}
		} else if (color != state->color) {
			/* Color changes wipes away any previous attributes
			 * which means underlines has to be added together with
			 * the color below so here we just update the state. */
			state->underline = underline;
		} else {
			/* Completely handle the underlining. */
			state->underline = underline;

			if (underline) {
				add_bytes_to_string(screen, "\033[4m", 4);
			} else {
				add_bytes_to_string(screen, "\033[24m", 5);
			}
		}
	}

	if (color != state->color) {
		unsigned char code[13];
		int length;

		state->color = color;

		code[0] = '\033';
		code[1] = '[';
		code[2] = '0';

		if (driver->colors) {
			code[3] = ';';
			code[4] = '3';
			code[5] = '0' + TERM_COLOR_FOREGROUND(color);

			code[8] = '0' + TERM_COLOR_BACKGROUND(color);
			if (!driver->trans || code[8] != '0') {
				code[6] = ';';
				code[7] = '4';
				length = 9;
			} else {
				length = 6;
			}
	 	} else if (use_negative_image(color)) {
			/* Flip the fore- and background colors for highlighing
			 * purposes. */
			code[3] = ';';
			code[4] = '7';
			length = 5;
		} else {
			length = 3;
		}

		if (underline) {
			code[length++] = ';';
			code[length++] = '4';
		}

		/* Check if the char should be rendered bold. */
		if (color & SCREEN_ATTR_BOLD) {
			code[length++] = ';';
			code[length++] = '1';
		}

		code[length++] = 'm';

		add_bytes_to_string(screen, code, length);
	}

	if (c >= ' ' && c != ASCII_DEL /* && c != 155*/) {
		if (border && driver->frame && c >= 176 && c < 224) {
			c = driver->frame[c - 176];
		}

		if (use_utf8_io(driver)) {
			int charset = driver->charsets[!!border];

			add_to_string(screen, cp2utf_8(charset, c));
		} else {
			add_char_to_string(screen, c);
		}
	} else {
		add_char_to_string(screen, ' ');
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

/* Updating of the driverinal screen is done by checking what needs to be updated
 * using the last screen. */
void
redraw_screen(struct terminal *term)
{
	struct screen_driver *driver = get_screen_driver(term);
	struct string image;
	register int y = 0;
	int prev_y = -1;
	struct screen_state state = { 0xFF, 0xFF, 0xFF };
	struct terminal_screen *screen = term->screen;
 	register struct screen_char *current;
 	register struct screen_char *pos;
 	register struct screen_char *prev_pos;

	if (!driver
	    || !screen
	    || !screen->dirty
	    || (term->master && is_blocked())
	    || !init_string(&image)) return;

	current = screen->last_image;
 	pos = screen->image;
 	prev_pos = NULL;

 	for (; y < term->y; y++) {
 		register int x = 0;

 		for (; x < term->x; x++, current++, pos++) {

			if (pos->color == current->color) {
				/* No update for exact match. */
				if (pos->data == current->data && pos->attr == current->attr)
					continue;
				/* Else if the color match and the data is ``space''. */
				if ((pos->data <= 1 || pos->data == ' ') &&
 				    (current->data <= 1 || current->data == ' '))
					continue;
			}

			/* Move the cursor when @prev_pos is more than 10 chars
			 * away. */
 			if (prev_y != y || prev_pos + 10 <= pos) {
 				add_cursor_move_to_string(&image, y + 1, x + 1);
 				prev_pos = pos;
				prev_y = y;
			}

			for (; prev_pos <= pos ; prev_pos++)
				print_char(&image, driver, prev_pos, &state);
		}
	}

	if (image.length) {
		if (driver->colors)
			add_bytes_to_string(&image, "\033[37;40m", 8);

		add_bytes_to_string(&image, "\033[0m", 4);

		/* If we ended in border state end the frame mode. */
		if (state.border && driver->frame_seqs) {
			struct frame_seq *seq = &driver->frame_seqs[0];

			add_bytes_to_string(&image, seq->src, seq->len);
		}
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
