/* Terminal screen drawing routines. */
/* $Id: screen.c,v 1.89 2003/10/02 00:06:16 jonas Exp $ */

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

#define TERM_STRING(str) INIT_STRING(str, sizeof(str) - 1)

#define add_term_string(str, tstr) \
	add_bytes_to_string(str, (tstr).source, (tstr).length)

static struct string m11_hack_frame_seqs[] = {
	/* end border: */	TERM_STRING("\033[10m"),
	/* begin border: */	TERM_STRING("\033[11m"),
};

static struct string vt100_frame_seqs[] = {
	/* end border: */	TERM_STRING("\x0f"),
	/* begin border: */	TERM_STRING("\x0e"),
};

static struct string underline_seqs[] = {
	/* begin underline: */	TERM_STRING("\033[24m"),
	/* end underline: */	TERM_STRING("\033[4m"),
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
	struct string *frame_seqs;

	/* The underline mode setup and teardown sequences. May be NULL. */
	struct string *underline;

	/* The color mode */
	enum color_mode color_mode;

	/* These are directly derived from the terminal options. */
	unsigned int trans:1;

	/* The terminal._template_ name. */
	unsigned char name[1]; /* XXX: Keep last! */
};

static struct screen_driver dumb_screen_driver = {
				NULL_LIST_HEAD,
	/* type: */		TERM_DUMB,
	/* charsets: */		{ -1, -1 },	/* No UTF8 I/O */
	/* frame: */		frame_dumb,
	/* frame_seqs: */	NULL,
	/* underline: */	underline_seqs,
	/* color_mode: */	COLOR_MODE_16,
	/* trans: */		1,
};

static struct screen_driver vt100_screen_driver = {
				NULL_LIST_HEAD,
	/* type: */		TERM_VT100,
	/* charsets: */		{ -1, -1 },	/* No UTF8 I/O */
	/* frame: */		frame_vt100,	/* No UTF8 I/O */
	/* frame_seqs: */	vt100_frame_seqs, /* No UTF8 I/O */
	/* underline: */	underline_seqs,
	/* color_mode: */	COLOR_MODE_16,
	/* trans: */		1,
};

static struct screen_driver linux_screen_driver = {
				NULL_LIST_HEAD,
	/* type: */		TERM_LINUX,
	/* charsets: */		{ -1, -1 },	/* No UTF8 I/O */
	/* frame: */		NULL,		/* No restrict_852 */
	/* frame_seqs: */	NULL,		/* No m11_hack */
	/* underline: */	underline_seqs,
	/* color_mode: */	COLOR_MODE_16,
	/* trans: */		1,
};

static struct screen_driver koi8_screen_driver = {
				NULL_LIST_HEAD,
	/* type: */		TERM_KOI8,
	/* charsets: */		{ -1, -1 },	/* No UTF8 I/O */
	/* frame: */		frame_koi,
	/* frame_seqs: */	NULL,
	/* underline: */	underline_seqs,
	/* color_mode: */	COLOR_MODE_16,
	/* trans: */		1,
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

	driver->color_mode = get_opt_int_tree(term_spec, "colors");
	driver->trans = get_opt_bool_tree(term_spec, "transparency");

	if (get_opt_bool_tree(term_spec, "underline")) {
		driver->underline = underline_seqs;
	} else {
		driver->underline = NULL;
	}

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

	/* One byte is reserved for name in struct screen_driver. */
	driver = mem_alloc(sizeof(struct screen_driver) + env_len);
	if (!driver) return NULL;

	memcpy(driver, screen_drivers[type], sizeof(struct screen_driver) - 1);
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

	foreach (driver, active_screen_drivers) {
		if (driver->type == type && !memcmp(driver->name, name, len)) {
			/* Some simple probably useless MRU ;) */
			if (driver != active_screen_drivers.next) {
				del_from_list(driver);
				add_to_list(active_screen_drivers, driver);
			}

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

#define compare_color(a, b)	((a) == (b))
#define copy_color(a, b)	((a) = (b))
#define use_utf8_io(driver)	((driver)->charsets[0] != -1)

/* Time critical section. */
static inline void
add_char16(struct string *screen, struct screen_driver *driver,
	   struct screen_char *ch, struct screen_state *state)
{
	unsigned char c = ch->data;
	unsigned char border = (ch->attr & SCREEN_ATTR_FRAME);
	unsigned char underline = (ch->attr & SCREEN_ATTR_UNDERLINE);

	if (border != state->border && driver->frame_seqs) {
		state->border = border;
		add_term_string(screen, driver->frame_seqs[!!border]);
	}

	if (underline != state->underline && driver->underline) {
		state->underline = underline;
		add_term_string(screen, driver->underline[!!underline]);
	}

	if (!compare_color(ch->color, state->color)) {
		copy_color(state->color, ch->color);

		add_bytes_to_string(screen, "\033[0", 3);

		if (driver->color_mode == COLOR_MODE_16) {
			static unsigned char code[6] = ";30;40";
			unsigned char color = ch->color;
			unsigned char bgcolor = TERM_COLOR_BACKGROUND(color);

			code[2] = '0' + TERM_COLOR_FOREGROUND(color);

			if (!driver->trans || bgcolor != 0) {
				code[5] = '0' + bgcolor;
				add_bytes_to_string(screen, code, 6);
			} else {
				add_bytes_to_string(screen, code, 3);
			}

		} else if (ch->attr & SCREEN_ATTR_STANDOUT) {
			/* Flip the fore- and background colors for highlighing
			 * purposes. */
			add_bytes_to_string(screen, ";7", 2);
		}

		if (underline && driver->underline) {
			add_bytes_to_string(screen, ";4", 2);
		}

		/* Check if the char should be rendered bold. */
		if (color & SCREEN_ATTR_BOLD) {
			add_bytes_to_string(screen, ";1", 2);
		}

		add_bytes_to_string(screen, "m", 1);
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


static inline void
add_chars16(struct string *image, struct terminal *term,
	    struct screen_state *state, struct screen_driver *driver)
{
	register struct screen_char *current = term->screen->last_image;
	register struct screen_char *pos = term->screen->image;
	register struct screen_char *prev_pos = NULL;
	register int y = 0;
	int prev_y = -1;

	for (; y < term->y; y++) {
		register int x = 0;

		for (; x < term->x; x++, current++, pos++) {

			if (compare_color(pos->color, current->color)) {
				/* No update for exact match. */
				if (pos->data == current->data
				    && pos->attr == current->attr)
					continue;

				/* Else if the color match and the data is ``space''. */
				if (pos->data <= ' ' && current->data <= ' ')
					continue;
			}

			/* Move the cursor when @prev_pos is more than 10 chars
			 * away. */
			if (prev_y != y || prev_pos + 10 <= pos) {
				add_cursor_move_to_string(image, y + 1, x + 1);
				prev_pos = pos;
				prev_y = y;
			}

			for (; prev_pos <= pos ; prev_pos++)
				add_char16(image, driver, prev_pos, state);
		}
	}
}

/* Updating of the terminal screen is done by checking what needs to be updated
 * using the last screen. */
void
redraw_screen(struct terminal *term)
{
	struct screen_driver *driver = get_screen_driver(term);
	struct string image;
	struct screen_state state = { 0xFF, 0xFF, 0xFF };
	struct terminal_screen *screen = term->screen;

	if (!driver
	    || !screen
	    || !screen->dirty
	    || (term->master && is_blocked())
	    || !init_string(&image)) return;

	switch (driver->color_mode) {
	case COLOR_MODE_MONO:
	case COLOR_MODE_16:
		add_chars16(&image, term, &state, driver);
		break;

	default:
		internal("Invalid color mode");
		return;
	}

	if (image.length) {
		if (driver->color_mode)
			add_bytes_to_string(&image, "\033[37;40m", 8);

		add_bytes_to_string(&image, "\033[0m", 4);

		/* If we ended in border state end the frame mode. */
		if (state.border && driver->frame_seqs) {
			add_term_string(&image, driver->frame_seqs[0]);
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

	copy_screen_chars(screen->last_image, screen->image, term->x * term->y);
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
