/* Support for keyboard interface */
/* $Id: kbd.c,v 1.93 2004/07/28 13:37:49 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <termios.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef __hpux__
#include <limits.h>
#define HPUX_PIPE	(len > PIPE_BUF || errno != EAGAIN)
#else
#define HPUX_PIPE	1
#endif

#include "elinks.h"

#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "osdep/ascii.h"
#include "osdep/osdep.h"
#include "terminal/hardio.h"
#include "terminal/kbd.h"
#include "terminal/mouse.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


#define OUT_BUF_SIZE	16384
#define IN_BUF_SIZE	16

#define TW_BUTT_LEFT	1
#define TW_BUTT_MIDDLE	2
#define TW_BUTT_RIGHT	4

struct itrm {
	int std_in;
	int std_out;
	int sock_in;
	int sock_out;
	int ctl_in;

	/* Input queue */
	unsigned char kqueue[IN_BUF_SIZE];
	int qlen;

	/* Output queue */
	unsigned char *ev_queue;
	int eqlen;

	int timer;			/* ESC timeout timer */
	struct termios t;		/* For restoring original attributes */
	void *mouse_h;			/* Mouse handle */
	unsigned char *orig_title;	/* For restoring window title */

	unsigned int blocked:1;		/* Whether it was blocked */
	unsigned int altscreen:1;	/* Whether to use alternate screen */
};

static struct itrm *ditrm = NULL;

static void free_trm(struct itrm *);
static void in_kbd(struct itrm *);
static void in_sock(struct itrm *);
static int process_queue(struct itrm *);

int
is_blocked(void)
{
	return ditrm && ditrm->blocked;
}


void
free_all_itrms(void)
{
	if (ditrm) free_trm(ditrm);
}


static void
write_ev_queue(struct itrm *itrm)
{
	int written;
	int qlen = int_min(itrm->eqlen, 128);

	assertm(qlen, "event queue empty");
	if_assert_failed return;

	written = safe_write(itrm->sock_out, itrm->ev_queue, qlen);
	if (written <= 0) {
		if (written < 0) free_trm(itrm); /* write error */
		return;
	}

	itrm->eqlen -= written;

	if (itrm->eqlen == 0) {
		set_handlers(itrm->sock_out,
			     get_handler(itrm->sock_out, H_READ),
			     NULL,
			     get_handler(itrm->sock_out, H_ERROR),
			     get_handler(itrm->sock_out, H_DATA));
	} else {
		assert(itrm->eqlen > 0);
		memmove(itrm->ev_queue, itrm->ev_queue + written, itrm->eqlen);
	}
}


static void
queue_event(struct itrm *itrm, unsigned char *data, int len)
{
	int w = 0;

	if (!len) return;

	if (!itrm->eqlen && can_write(itrm->sock_out)) {
		w = safe_write(itrm->sock_out, data, len);
		if (w <= 0 && HPUX_PIPE) {
			/* free_trm(itrm); */
			register_bottom_half((void (*)(void *)) free_trm, itrm);
			return;
		}
	}

	if (w < len) {
		int left = len - w;
		unsigned char *c = mem_realloc(itrm->ev_queue,
					       itrm->eqlen + left);

		if (!c) {
			free_trm(itrm);
			return;
		}

		itrm->ev_queue = c;
		memcpy(itrm->ev_queue + itrm->eqlen, data + w, left);
		itrm->eqlen += left;
		set_handlers(itrm->sock_out,
			     get_handler(itrm->sock_out, H_READ),
			     (void (*)(void *)) write_ev_queue,
			     (void (*)(void *)) free_trm, itrm);
	}
}


void
kbd_ctrl_c(void)
{
	struct term_event ev = INIT_TERM_EVENT(EVENT_KBD, KBD_CTRL_C, 0, 0);

	if (!ditrm) return;
	queue_event(ditrm, (unsigned char *) &ev, sizeof(struct term_event));
}

#define write_sequence(fd, seq) \
	hard_write(fd, seq, sizeof(seq) / sizeof(unsigned char) - 1)

#define INIT_TERMINAL_SEQ	"\033)0\0337"	/* Special Character and Line Drawing Set, Save Cursor */
#define INIT_TWIN_MOUSE_SEQ	"\033[?9h"	/* Send MIT Mouse Row & Column on Button Press */
#define INIT_XWIN_MOUSE_SEQ	"\033[?1000h"	/* Send Mouse X & Y on button press and release */
#define INIT_ALT_SCREEN_SEQ	"\033[?47h"	/* Use Alternate Screen Buffer */

static void
send_init_sequence(int h, int altscreen)
{
	write_sequence(h, INIT_TERMINAL_SEQ);

	/* If alternate screen is supported switch to it. */
	if (altscreen) {
		write_sequence(h, INIT_ALT_SCREEN_SEQ);
	}
#ifdef CONFIG_MOUSE
	write_sequence(h, INIT_TWIN_MOUSE_SEQ);
	write_sequence(h, INIT_XWIN_MOUSE_SEQ);
#endif
}

#define DONE_CLS_SEQ		"\033[2J"	/* Erase in Display, Clear All */
#define DONE_TERMINAL_SEQ	"\0338\r \b"	/* Restore Cursor (DECRC) + ??? */
#define DONE_TWIN_MOUSE_SEQ	"\033[?9l"	/* Don't Send MIT Mouse Row & Column on Button Press */
#define DONE_XWIN_MOUSE_SEQ	"\033[?1000l"	/* Don't Send Mouse X & Y on button press and release */
#define DONE_ALT_SCREEN_SEQ	"\033[?47l"	/* Use Normal Screen Buffer */

static void
send_done_sequence(int h, int altscreen)
{
	write_sequence(h, DONE_CLS_SEQ);

#ifdef CONFIG_MOUSE
	/* This is a hack to make xterm + alternate screen working,
	 * if we send only DONE_XWIN_MOUSE_SEQ, mouse is not totally
	 * released it seems, in rxvt and xterm... --Zas */
	write_sequence(h, DONE_TWIN_MOUSE_SEQ);
	write_sequence(h, DONE_XWIN_MOUSE_SEQ);
#endif

	/* Switch from alternate screen. */
	if (altscreen) {
		write_sequence(h, DONE_ALT_SCREEN_SEQ);
	}

	write_sequence(h, DONE_TERMINAL_SEQ);
}

#undef write_sequence

void
resize_terminal(void)
{
	struct term_event ev = INIT_TERM_EVENT(EVENT_RESIZE, 0, 0, 0);
	int width, height;

	if (get_terminal_size(ditrm->std_out, &width, &height)) return;
	ev.x = width;
	ev.y = height;
	queue_event(ditrm, (char *) &ev, sizeof(struct term_event));
}


static int
setraw(int fd, struct termios *p)
{
	struct termios t;

	memset(&t, 0, sizeof(struct termios));
	if (tcgetattr(fd, &t)) return -1;

	if (p) memcpy(p, &t, sizeof(struct termios));

	elinks_cfmakeraw(&t);
	t.c_lflag |= ISIG;
#ifdef TOSTOP
	t.c_lflag |= TOSTOP;
#endif
	t.c_oflag |= OPOST;
	if (tcsetattr(fd, TCSANOW, &t)) return -1;

	return 0;
}

void
handle_trm(int std_in, int std_out, int sock_in, int sock_out, int ctl_in,
	   void *init_string, int init_len, int remote)
{
	int magic = remote ? INTERLINK_REMOTE_MAGIC : INTERLINK_NORMAL_MAGIC;
	int session_info = remote ? remote : get_cmd_opt_int("base-session");
	struct itrm *itrm;
	int width, height;
	int terminal_size_error = get_terminal_size(ctl_in, &width, &height);
	struct terminal_info info = {
		INIT_TERM_EVENT(EVENT_INIT, width, height, 0),
		"",
		"",
		get_system_env(),
		init_len,
		session_info,
		magic,
	};
	unsigned char *ts;

	if (terminal_size_error) {
		ERROR(G_("Could not get terminal size"));
		return;
	}

	itrm = mem_calloc(1, sizeof(struct itrm));
	if (!itrm) return;

	ditrm = itrm;
	itrm->std_in = std_in;
	itrm->std_out = std_out;
	itrm->sock_in = sock_in;
	itrm->sock_out = sock_out;
	itrm->ctl_in = ctl_in;
	itrm->timer = -1;

	/* FIXME: Combination altscreen + xwin does not work as it should,
	 * mouse clicks are reportedly partially ignored. */
	if (info.system_env & (ENV_SCREEN | ENV_XWIN))
		itrm->altscreen = 1;

	if (ctl_in >= 0) setraw(ctl_in, &itrm->t);
	set_handlers(std_in, (void (*)(void *)) in_kbd,
		     NULL, (void (*)(void *)) free_trm, itrm);

	if (sock_in != std_out)
		set_handlers(sock_in, (void (*)(void *)) in_sock,
			     NULL, (void (*)(void *)) free_trm, itrm);

	handle_terminal_resize(ctl_in, resize_terminal);

	ts = getenv("TERM");
	if (ts) {
		int i;

		for (i = 0; ts[i] != 0 && i < MAX_TERM_LEN; i++)
			info.name[i] = isident(ts[i]) ? ts[i] : '-';
	}

	ts = get_cwd();
	if (ts) {
		memcpy(info.cwd, ts, int_min(strlen(ts), MAX_CWD_LEN));
		mem_free(ts);
	}

	queue_event(itrm, (char *) &info, TERMINAL_INFO_SIZE);
	queue_event(itrm, (char *) init_string, init_len);
	send_init_sequence(std_out, itrm->altscreen);

	itrm->mouse_h = handle_mouse(0, (void (*)(void *, unsigned char *, int)) queue_event, itrm);
}


static void
unblock_itrm_x(void *h)
{
	close_handle(h);
	if (!ditrm) return;
	unblock_itrm(0);
	resize_terminal();
}


int
unblock_itrm(int fd)
{
	struct itrm *itrm = ditrm;

	if (!itrm) return -1;

	if (itrm->ctl_in >= 0 && setraw(itrm->ctl_in, NULL)) return -1;
	itrm->blocked = 0;
	send_init_sequence(itrm->std_out, itrm->altscreen);

	set_handlers(itrm->std_in, (void (*)(void *)) in_kbd, NULL,
		     (void (*)(void *)) free_trm, itrm);

	resume_mouse(itrm->mouse_h);

	handle_terminal_resize(itrm->ctl_in, resize_terminal);
	unblock_stdin();

	return 0;
}


void
block_itrm(int fd)
{
	struct itrm *itrm = ditrm;

	if (!itrm) return;

	itrm->blocked = 1;
	block_stdin();
	unhandle_terminal_resize(itrm->ctl_in);
	send_done_sequence(itrm->std_out, itrm->altscreen);
	tcsetattr(itrm->ctl_in, TCSANOW, &itrm->t);
	set_handlers(itrm->std_in, NULL, NULL,
		     (void (*)(void *)) free_trm, itrm);
	suspend_mouse(itrm->mouse_h);
}


static void
free_trm(struct itrm *itrm)
{
	if (!itrm) return;

	if (itrm->orig_title) {
		set_window_title(itrm->orig_title);
		mem_free(itrm->orig_title);
		itrm->orig_title = NULL;
	}

	unhandle_terminal_resize(itrm->ctl_in);
	unhandle_mouse(itrm->mouse_h);
	send_done_sequence(itrm->std_out,itrm->altscreen);
	tcsetattr(itrm->ctl_in, TCSANOW, &itrm->t);

	set_handlers(itrm->std_in, NULL, NULL, NULL, NULL);
	set_handlers(itrm->sock_in, NULL, NULL, NULL, NULL);
	set_handlers(itrm->std_out, NULL, NULL, NULL, NULL);
	set_handlers(itrm->sock_out, NULL, NULL, NULL, NULL);

	if (itrm->timer != -1)
		kill_timer(itrm->timer);

	if (itrm == ditrm) ditrm = NULL;
	mem_free_if(itrm->ev_queue);
	mem_free(itrm);
}

/* Resize terminal to dimensions specified by @text string.
 * @text should look like 'width,height' where width and height are integers. */
static inline void
resize_terminal_from_str(unsigned char *text)
{
	unsigned char *p;
	int width, height;

	assert(text && *text);
	if_assert_failed return;

	p = strchr(text, ',');
	if (!p) return;

	*p++ = '\0';
	width = atoi(text);
	height = atoi(p);

	resize_window(width, height);
	resize_terminal();
}

void
dispatch_special(unsigned char *text)
{
	switch (text[0]) {
		case TERM_FN_TITLE:
			if (ditrm && !ditrm->orig_title)
				ditrm->orig_title = get_window_title();
			set_window_title(text + 1);
			break;
		case TERM_FN_RESIZE:
			resize_terminal_from_str(text + 1);
			break;
	}
}

static void inline
safe_hard_write(int fd, unsigned char *buf, int len)
{
	if (is_blocked()) return;

	want_draw();
	hard_write(fd, buf, len);
	done_draw();
}

static void
in_sock(struct itrm *itrm)
{
	struct string path;
	struct string delete;
	char ch;
	int fg;
	int bytes_read, i, p;
	unsigned char buf[OUT_BUF_SIZE];

	bytes_read = safe_read(itrm->sock_in, buf, OUT_BUF_SIZE);
	if (bytes_read <= 0) goto free_and_return;

qwerty:
	for (i = 0; i < bytes_read; i++)
		if (!buf[i])
			goto has_nul_byte;

	safe_hard_write(itrm->std_out, buf, bytes_read);
	return;

has_nul_byte:
	if (i) safe_hard_write(itrm->std_out, buf, i);

	i++;
	assert(OUT_BUF_SIZE - i > 0);
	memmove(buf, buf + i, OUT_BUF_SIZE - i);
	bytes_read -= i;
	p = 0;

#define RD(xx) {							\
		unsigned char cc;					\
									\
		if (p < bytes_read)					\
			cc = buf[p++];					\
		else if ((hard_read(itrm->sock_in, &cc, 1)) <= 0)	\
			goto free_and_return;				\
		xx = cc;						\
	}

	RD(fg);

	if (!init_string(&path)) goto free_and_return;

	while (1) {
		RD(ch);
		if (!ch) break;
		add_char_to_string(&path, ch);
	}

	if (!init_string(&delete)) {
		done_string(&path);
		goto free_and_return;
	}

	while (1) {
		RD(ch);
		if (!ch) break;
		add_char_to_string(&delete, ch);
	}

#undef RD

	if (!*path.source) {
		dispatch_special(delete.source);

	} else {
		int blockh;
		unsigned char *param;
		int path_len, del_len, param_len;

		if (is_blocked() && fg) {
			if (*delete.source) unlink(delete.source);
			goto nasty_thing;
		}

		path_len = path.length;
		del_len = delete.length;
		param_len = path_len + del_len + 3;

		param = mem_alloc(param_len);
		if (!param) goto nasty_thing;

		param[0] = fg;
		memcpy(param + 1, path.source, path_len + 1);
		memcpy(param + 1 + path_len + 1, delete.source, del_len + 1);

		if (fg == 1) block_itrm(itrm->ctl_in);

		blockh = start_thread((void (*)(void *, int)) exec_thread,
				      param, param_len);
		if (blockh == -1) {
			if (fg == 1)
				unblock_itrm(itrm->ctl_in);
			mem_free(param);
			goto nasty_thing;
		}

		mem_free(param);

		if (fg == 1) {
			set_handlers(blockh, (void (*)(void *)) unblock_itrm_x,
				     NULL, (void (*)(void *)) unblock_itrm_x,
				     (void *) blockh);
			/* block_itrm(itrm->ctl_in); */
		} else {
			set_handlers(blockh, close_handle, NULL, close_handle,
				     (void *) blockh);
		}
	}

nasty_thing:
	done_string(&path);
	done_string(&delete);
	assert(OUT_BUF_SIZE - p > 0);
	memmove(buf, buf + p, OUT_BUF_SIZE - p);
	bytes_read -= p;

	goto qwerty;

free_and_return:
	free_trm(itrm);
}


static void
kbd_timeout(struct itrm *itrm)
{
	struct term_event ev = INIT_TERM_EVENT(EVENT_KBD, KBD_ESC, 0, 0);

	itrm->timer = -1;

	if (can_read(itrm->std_in)) {
		in_kbd(itrm);
		return;
	}

	assertm(itrm->qlen, "timeout on empty queue");
	if_assert_failed return;

	queue_event(itrm, (char *) &ev, sizeof(struct term_event));

	if (--itrm->qlen)
		memmove(itrm->kqueue, itrm->kqueue + 1, itrm->qlen);

	while (process_queue(itrm));
}


/* Returns the length of the escape sequence */
static inline int
get_esc_code(unsigned char *str, int len, unsigned char *code, int *num)
{
	int pos;

	*num = 0;
	*code = '\0';

	for (pos = 2; pos < len; pos++) {
		if (!isdigit(str[pos]) || pos > 7) {
			*code = str[pos];

			return pos + 1;
		}
		*num = *num * 10 + str[pos] - '0';
	}

	return 0;
}


struct key {
	int x, y;
};

static struct key os2xtd[256] = {
#include "terminal/key.inc"
};

/* Define it to dump queue content in a readable form,
 * it may help to determine terminal sequences, and see what goes on. --Zas */
/* #define DEBUG_ITRM_QUEUE */

#ifdef DEBUG_ITRM_QUEUE
#include <ctype.h>	/* isprint() isspace() */
#endif

#ifdef CONFIG_MOUSE
/* Returns length of the escape sequence or -1 if the caller needs to set up
 * the ESC delay timer. */
static int
decode_terminal_mouse_escape_sequence(struct itrm *itrm, struct term_event *ev,
				      int el, int v)
{
	static int xterm_button = -1;
	struct term_event_mouse *mouse = &ev->info.mouse;

	if (itrm->qlen - el < 3)
		return -1;

	if (v == 5) {
		if (xterm_button == -1)
			xterm_button = 0;
		if (itrm->qlen - el < 5)
			return -1;

		ev->x = (unsigned char)(itrm->kqueue[el+1]) - ' ' - 1
		       + ((int)((unsigned char)(itrm->kqueue[el+2]) - ' ' - 1) << 7);

		if (ev->x & (1 << 13)) ev->x = 0; /* ev->x |= ~0 << 14; */

		ev->y = (unsigned char)(itrm->kqueue[el+3]) - ' ' - 1
		       + ((int)((unsigned char)(itrm->kqueue[el+4]) - ' ' - 1) << 7);

		if (ev->y & (1 << 13)) ev->y = 0; /* ev->y |= ~0 << 14; */

		switch ((itrm->kqueue[el] - ' ') ^ xterm_button) { /* Every event changes only one bit */
		    case TW_BUTT_LEFT:   mouse->button = B_LEFT | ( (xterm_button & TW_BUTT_LEFT) ? B_UP : B_DOWN ); break;
		    case TW_BUTT_MIDDLE: mouse->button = B_MIDDLE | ( (xterm_button & TW_BUTT_MIDDLE) ? B_UP :  B_DOWN ); break;
		    case TW_BUTT_RIGHT:  mouse->button = B_RIGHT | ( (xterm_button & TW_BUTT_RIGHT) ? B_UP : B_DOWN ); break;
		    case 0: ev->info.mouse.button = B_DRAG;
		    /* default : Twin protocol error */
		}
		xterm_button = itrm->kqueue[el] - ' ';
		el += 5;
	} else {
		/* See terminal/mouse.h about details of the mouse reporting
		 * protocol and {struct term_event_mouse->button} bitmask
		 * structure. */
		ev->x = itrm->kqueue[el+1] - ' ' - 1;
		ev->y = itrm->kqueue[el+2] - ' ' - 1;

		/* There are rumours arising from remnants of code dating to
		 * the ancient Mikulas' times that bit 4 indicated B_DRAG.
		 * However, I didn't find on what terminal it should be ever
		 * supposed to work and it conflicts with wheels. So I removed
		 * the last remnants of the code as well. --pasky */

		mouse->button = (itrm->kqueue[el] & 7) | B_DOWN;
		/* smartglasses1 - rxvt wheel: */
		if (mouse->button == 3 && xterm_button != -1) {
			mouse->button = xterm_button | B_UP;
		}
		/* xterm wheel: */
		if ((itrm->kqueue[el] & 96) == 96) {
			mouse->button = (itrm->kqueue[el] & 1) ? B_WHEEL_DOWN : B_WHEEL_UP;
		}

		xterm_button = -1;
		/* XXX: Eterm/aterm uses rxvt-like reporting, but sends the
		 * release sequence for wheel. rxvt itself sends only press
		 * sequence. Since we can't reliably guess what we're talking
		 * with from $TERM, we will rather support Eterm/aterm, as in
		 * rxvt, at least each second wheel up move will work. */
		if (check_mouse_action(ev, B_DOWN))
#if 0
			    && !(getenv("TERM") && strstr("rxvt", getenv("TERM"))
				 && (ev->b & BM_BUTT) >= B_WHEEL_UP))
#endif
			xterm_button = get_mouse_button(ev);

		el += 3;
	}
	ev->ev = EVENT_MOUSE;

	return el;
}
#endif /* CONFIG_MOUSE */

/* Returns length of the escape sequence or -1 if the caller needs to set up
 * the ESC delay timer. */
static int
decode_terminal_escape_sequence(struct itrm *itrm, struct term_event *ev)
{
	unsigned char c;
	int v;
	int el;

	if (itrm->qlen < 3) return -1;

	if (itrm->kqueue[2] == '[') {
		if (itrm->qlen >= 4
		    && itrm->kqueue[3] >= 'A'
		    && itrm->kqueue[3] <= 'L') {
			ev->x = KBD_F1 + itrm->kqueue[3] - 'A';
			return 4;
		}

		return -1;
	}

	el = get_esc_code(itrm->kqueue, itrm->qlen, &c, &v);
#ifdef DEBUG_ITRM_QUEUE
	fprintf(stderr, "esc code: %c v=%d c=%c el=%d\n", itrm->kqueue[1], v, c, el);
	fflush(stderr);
#endif

	switch (c) {
	case 0: return -1;
	case 'A': ev->x = KBD_UP; break;
	case 'B': ev->x = KBD_DOWN; break;
	case 'C': ev->x = KBD_RIGHT; break;
	case 'D': ev->x = KBD_LEFT; break;
	case 'F':
	case 'e': ev->x = KBD_END; break;
	case 'H': ev->x = KBD_HOME; break;
	case 'I': ev->x = KBD_PAGE_UP; break;
	case 'G': ev->x = KBD_PAGE_DOWN; break;

	case 'z': switch (v) {
		case 247: ev->x = KBD_INS; break;
		case 214: ev->x = KBD_HOME; break;
		case 220: ev->x = KBD_END; break;
		case 216: ev->x = KBD_PAGE_UP; break;
		case 222: ev->x = KBD_PAGE_DOWN; break;
		case 249: ev->x = KBD_DEL; break;
		} break;

	case '~': switch (v) {
		case 1: ev->x = KBD_HOME; break;
		case 2: ev->x = KBD_INS; break;
		case 3: ev->x = KBD_DEL; break;
		case 4: ev->x = KBD_END; break;
		case 5: ev->x = KBD_PAGE_UP; break;
		case 6: ev->x = KBD_PAGE_DOWN; break;
		case 7: ev->x = KBD_HOME; break;
		case 8: ev->x = KBD_END; break;

		case 11: ev->x = KBD_F1; break;
		case 12: ev->x = KBD_F2; break;
		case 13: ev->x = KBD_F3; break;
		case 14: ev->x = KBD_F4; break;
		case 15: ev->x = KBD_F5; break;

		case 17: ev->x = KBD_F6; break;
		case 18: ev->x = KBD_F7; break;
		case 19: ev->x = KBD_F8; break;
		case 20: ev->x = KBD_F9; break;
		case 21: ev->x = KBD_F10; break;

		case 23: ev->x = KBD_F11; break;
		case 24: ev->x = KBD_F12; break;

		/* Give preference to F11 and F12 over shifted F1 and F2. */
		/*
		case 23: ev->x = KBD_F1; ev->y = KBD_SHIFT; break;
		case 24: ev->x = KBD_F2; ev->y = KBD_SHIFT; break;
		*/

		case 25: ev->x = KBD_F3; ev->y = KBD_SHIFT; break;
		case 26: ev->x = KBD_F4; ev->y = KBD_SHIFT; break;

		case 28: ev->x = KBD_F5; ev->y = KBD_SHIFT; break;
		case 29: ev->x = KBD_F6; ev->y = KBD_SHIFT; break;

		case 31: ev->x = KBD_F7; ev->y = KBD_SHIFT; break;
		case 32: ev->x = KBD_F8; ev->y = KBD_SHIFT; break;
		case 33: ev->x = KBD_F9; ev->y = KBD_SHIFT; break;
		case 34: ev->x = KBD_F10; ev->y = KBD_SHIFT; break;

		} break;

	case 'R': resize_terminal(); break;
	case 'M':
#ifdef CONFIG_MOUSE
		el = decode_terminal_mouse_escape_sequence(itrm, ev, el, v);
#endif /* CONFIG_MOUSE */
		break;
	}

	return el;
}

/* I feeeeeel the neeeed ... to rewrite this ... --pasky */
/* Just Do it ! --Zas */
static int
process_queue(struct itrm *itrm)
{
	struct term_event ev = INIT_TERM_EVENT(EVENT_KBD, -1, 0, 0);
	int el = 0;

	if (!itrm->qlen) goto end;

#ifdef DEBUG_ITRM_QUEUE
	{
		int i;

		/* Dump current queue in a readable form to stderr. */
		for (i = 0; i < itrm->qlen; i++)
			if (itrm->kqueue[i] == ASCII_ESC)
				fprintf(stderr, "ESC ");
			else if (isprint(itrm->kqueue[i]) && !isspace(itrm->kqueue[i]))
				fprintf(stderr, "%c ", itrm->kqueue[i]);
			else
				fprintf(stderr, "0x%02x ", itrm->kqueue[i]);

		fprintf(stderr, "\n");
		fflush(stderr);
	}
#endif /* DEBUG_ITRM_QUEUE */

	if (itrm->kqueue[0] == ASCII_ESC) {
		if (itrm->qlen < 2) goto ret;
		if (itrm->kqueue[1] == '[' || itrm->kqueue[1] == 'O') {
			el = decode_terminal_escape_sequence(itrm, &ev);

			if (el == -1) goto ret;

		} else {
			el = 2;

			if (itrm->kqueue[1] == ASCII_ESC) {
				if (itrm->qlen >= 3 &&
				    (itrm->kqueue[2] == '[' ||
				     itrm->kqueue[2] == 'O')) {
					el = 1;
				}
				ev.x = KBD_ESC;
				goto l2;
			}

			ev.x = itrm->kqueue[1];
			ev.y = KBD_ALT;
			goto l2;
		}

		goto l1;

	} else if (itrm->kqueue[0] == 0) {
		if (itrm->qlen < 2) goto ret;
		ev.x = os2xtd[itrm->kqueue[1]].x;
		if (!ev.x) ev.x = -1;
		ev.y = os2xtd[itrm->kqueue[1]].y;
		el = 2;
		goto l1;
	}
	el = 1;
	ev.x = itrm->kqueue[0];

l2:
	if (ev.x == ASCII_TAB) ev.x = KBD_TAB;
	else if (ev.x == ASCII_BS || ev.x == ASCII_DEL) ev.x = KBD_BS;
	else if (ev.x == ASCII_LF || ev.x == ASCII_CR) ev.x = KBD_ENTER;
	if (ev.x < ' ') {
		ev.x += 'A' - 1;
		ev.y = KBD_CTRL;
	}

l1:
	assertm(itrm->qlen >= el, "event queue underflow");
	if_assert_failed { itrm->qlen = el; }

	itrm->qlen -= el;

	if (ev.x != -1)
		queue_event(itrm, (char *) &ev, sizeof(struct term_event));

	if (itrm->qlen)
		memmove(itrm->kqueue, itrm->kqueue + el, itrm->qlen);

end:
	if (itrm->qlen < IN_BUF_SIZE)
		set_handlers(itrm->std_in, (void (*)(void *)) in_kbd, NULL,
			     (void (*)(void *)) free_trm, itrm);
	return el;

ret:
	itrm->timer = install_timer(ESC_TIMEOUT, (void (*)(void *)) kbd_timeout,
				    itrm);

	return 0;
}


static void
in_kbd(struct itrm *itrm)
{
	int r;

	if (!can_read(itrm->std_in)) return;

	if (itrm->timer != -1) {
		kill_timer(itrm->timer);
		itrm->timer = -1;
	}

	if (itrm->qlen >= IN_BUF_SIZE) {
		set_handlers(itrm->std_in, NULL, NULL,
			     (void (*)(void *)) free_trm, itrm);
		while (process_queue(itrm));
		return;
	}

	r = safe_read(itrm->std_in, itrm->kqueue + itrm->qlen,
		      IN_BUF_SIZE - itrm->qlen);
	if (r <= 0) {
		free_trm(itrm);
		return;
	}

	itrm->qlen += r;
	if (itrm->qlen > IN_BUF_SIZE) {
		ERROR(G_("Too many bytes read from the itrm!"));
		itrm->qlen = IN_BUF_SIZE;
	}

	while (process_queue(itrm));
}
