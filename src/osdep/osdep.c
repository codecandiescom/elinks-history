/* Features which vary with the OS */
/* $Id: osdep.c,v 1.106 2003/10/27 02:04:50 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_IO_H
#include <io.h> /* For win32 && set_bin(). */
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_SIGNAL_H
#include <sys/signal.h>
#endif
#include <sys/types.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#include <termios.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#if defined(HAVE_LIBGPM) && defined(HAVE_GPM_H)
#define USE_GPM
#endif

#ifdef USE_GPM
#include <gpm.h>
#endif

#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif


#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "lowlevel/signals.h"
#include "osdep/os_dep.h"
#include "sched/session.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


/* Set a file descriptor to non-blocking mode. It returns a non-zero value
 * on error. */
int
set_nonblocking_fd(int fd)
{
#if defined(O_NONBLOCK) || defined(O_NDELAY)
	int flags = fcntl(fd, F_GETFL, 0);

	if (flags < 0) return -1;
#if defined(O_NONBLOCK)
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	return fcntl(fd, F_SETFL, flags | O_NDELAY);
#endif

#elif defined(FIONBIO)
	int flag = 1;

	return ioctl(fd, FIONBIO, &flag);
#else
	return 0;
#endif
}

/* Set a file descriptor to blocking mode. It returns a non-zero value on
 * error. */
int
set_blocking_fd(int fd)
{
#if defined(O_NONBLOCK) || defined(O_NDELAY)
	int flags = fcntl(fd, F_GETFL, 0);

	if (flags < 0) return -1;
#if defined(O_NONBLOCK)
	return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
#else
	return fcntl(fd, F_SETFL, flags & ~O_NDELAY);
#endif

#elif defined(FIONBIO)
	int flag = 0;

	return ioctl(fd, FIONBIO, &flag);
#else
	return 0;
#endif
}

int
get_e(unsigned char *env)
{
	char *v = getenv(env);

	return (v ? atoi(v) : 0);
}


/* Terminal size */

#if defined(UNIX) || defined(BEOS) || defined(RISCOS) || defined(WIN32)

static void
sigwinch(void *s)
{
	((void (*)(void))s)();
}

void
handle_terminal_resize(int fd, void (*fn)(void))
{
	install_signal_handler(SIGWINCH, sigwinch, fn, 0);
}

void
unhandle_terminal_resize(int fd)
{
	install_signal_handler(SIGWINCH, NULL, NULL, 0);
}

int
get_terminal_size(int fd, int *x, int *y)
{
	struct winsize ws;

	if (!x || !y) return -1;
	if (ioctl(1, TIOCGWINSZ, &ws) != -1) {
		*x = ws.ws_col;
		*y = ws.ws_row;
	}
	if (!*x) {
		*x = get_e("COLUMNS");
		if (!*x) *x = 80;
	}
	if (!*y) {
		*y = get_e("LINES");
		if (!*y) *y = 24;
	}

	return 0;
}

#endif

/* Pipe */

#if defined(UNIX) || defined(BEOS) || defined(RISCOS)

void
set_bin(int fd)
{
}

int
c_pipe(int *fd)
{
	return pipe(fd);
}

#elif defined(OS2) || defined(WIN32)

void
set_bin(int fd)
{
	setmode(fd, O_BINARY);
}

int
c_pipe(int *fd)
{
	int r = pipe(fd);

	if (!r) {
		set_bin(fd[0]);
		set_bin(fd[1]);
	}

	return r;
}

#endif

/* Filename */

int
check_file_name(unsigned char *file)
{
	return 1;		/* !!! FIXME */
}

/* Exec */

int
is_twterm(void) /* Check if it make sense to call a twterm. */
{
	static int tw = -1;

	if (tw == -1) tw = !!getenv("TWDISPLAY");

	return tw;
}


#if defined(UNIX) || defined(WIN32)

int
is_xterm(void)
{
	static int xt = -1;

	if (xt == -1) {
		unsigned char *display = getenv("DISPLAY");

		xt = (display && *display);
	}

	return xt;
}

#elif defined(RISCOS)

int
is_xterm(void)
{
       return 1;
}

#endif

unsigned int resize_count = 0;

#if defined(UNIX) || defined(WIN32) || defined(BEOS) || defined(RISCOS)

#if defined(BEOS) && defined(HAVE_SETPGID)

#elif defined(WIN32)

#else

int
exe(unsigned char *path)
{
	return system(path);
}

#endif

unsigned char *
get_clipboard_text(void)	/* !!! FIXME */
{
	unsigned char *ret = mem_alloc(1);

	if (ret) ret[0] = 0;

	return ret;
}

void
set_clipboard_text(unsigned char *data)
{
	/* !!! FIXME */
}

/* Set xterm-like term window's title. */
void
set_window_title(unsigned char *title)
{
	unsigned char *s;
	int xsize, ysize;
	register int j = 0;

	/* Check if we're in a xterm-like terminal. */
	if (!is_xterm()) return;

	/* Retrieve terminal dimensions. */
	if (get_terminal_size(0, &xsize, &ysize)) return;

	/* Check if terminal width is reasonnable. */
	if (xsize < 1 || xsize > 1024) return;

	/* Allocate space for title + 3 ending points + null char. */
	s = (unsigned char *) mem_alloc((xsize + 3 + 1) * sizeof(unsigned char));
	if (!s) return;

	/* Copy title to s if different from NULL */
	if (title) {
		register int i;

		/* We limit title length to terminal width and ignore control
		 * chars if any. Note that in most cases window decoration
		 * reduces printable width, so it's just a precaution. */
		for (i = 0; title[i] && i < xsize; i++) {
			if (title[i] >= ' ')
				s[j++] = title[i];
			else
				s[j++] = ' ';
		}

		/* If title is truncated, add "..." */
		if (i == xsize) {
			s[j++] = '.';
			s[j++] = '.';
			s[j++] = '.';
		}
	}
	s[j] = '\0';

	/* Send terminal escape sequence + title string */
	printf("\033]0;%s\a", s);
	fflush(stdout);

	mem_free(s);
}

#ifdef HAVE_X11
static int x_error = 0;

static int
catch_x_error(void)
{
	x_error = 1;
	return 0;
}
#endif

unsigned char *
get_window_title(void)
{
#ifdef HAVE_X11
	/* Following code is stolen from our beloved vim. */
	unsigned char *winid;
	Display *display;
	Window window, root, parent, *children;
	XTextProperty text_prop;
	Status status;
	unsigned int num_children;
	unsigned char *ret = NULL;

	if (!is_xterm())
		return NULL;

	winid = getenv("WINDOWID");
	if (!winid)
		return NULL;
	window = (Window) atol(winid);
	if (!window)
		return NULL;

	display = XOpenDisplay(NULL);
	if (!display)
		return NULL;

	/* If WINDOWID is bad, we don't want X to abort us. */
	x_error = 0;
	XSetErrorHandler((int (*)(Display *, XErrorEvent *))catch_x_error);

	status = XGetWMName(display, window, &text_prop);
	/* status = XGetWMIconName(x11_display, x11_window, &text_prop); */
	while (!x_error && (!status || !text_prop.value)) {
		if (!XQueryTree(display, window, &root, &parent, &children, &num_children))
			break;
		if (children)
			XFree((void *) children);
		if (parent == root || parent == 0)
			break;
		window = parent;
		status = XGetWMName(display, window, &text_prop);
	}

	if (!x_error && status && text_prop.value) {
		ret = stracpy(text_prop.value);
		XFree(text_prop.value);
	}

	XCloseDisplay(display);

	return ret;
#else
	return NULL;
#endif
}

int
resize_window(int x, int y)
{
	return -1;
}

#endif

/* Threads */

#if defined(HAVE_BEGINTHREAD) || defined(BEOS)

struct tdata {
	void (*fn)(void *, int);
	int h;
	unsigned char data[1];
};

void
bgt(struct tdata *t)
{
	signal(SIGPIPE, SIG_IGN);
	t->fn(t->data, t->h);
	write(t->h, "x", 1);
	close(t->h);
	free(t);
}

#endif

#if defined(UNIX) || defined(OS2) || defined(RISCOS)

void
terminate_osdep(void)
{
}

#endif

#ifndef BEOS

void
block_stdin(void)
{
}

void
unblock_stdin(void)
{
}

#endif

#if defined(BEOS)

#elif defined(HAVE_BEGINTHREAD)

#else /* HAVE_BEGINTHREAD */

int
start_thread(void (*fn)(void *, int), void *ptr, int l)
{
	int p[2];
	int f;

	if (c_pipe(p) < 0) return -1;
	if (set_nonblocking_fd(p[0]) < 0) return -1;
	if (set_nonblocking_fd(p[1]) < 0) return -1;

	f = fork();
	if (!f) {
		struct terminal *term;

		/* Close input in this thread; otherwise, if it will live
		 * longer than its parent, it'll block the terminal until it'll
		 * quit as well; this way it will hopefully just die unseen and
		 * in background, causing no trouble. */
		/* Particularly, when async dns resolving was in progress and
		 * someone quitted ELinks, it could make a delay before the
		 * terminal would be really freed and returned to shell. */
		foreach (term, terminals)
			if (term->fdin > 0)
				close(term->fdin);

		close(p[0]);
		fn(ptr, p[1]);
		write(p[1], "x", 1);
		close(p[1]);
		/* We use _exit() here instead of exit(), see
		 * http://www.erlenstar.demon.co.uk/unix/faq_2.html#SEC6 for
		 * reasons. Fixed by Sven Neumann <sven@convergence.de>. */
		_exit(0);
	}
	if (f == -1) {
		close(p[0]);
		close(p[1]);
		return -1;
	}

	close(p[1]);
	return p[0];
}

#endif

#ifndef USING_OS2_MOUSE
void
want_draw(void)
{
}

void
done_draw(void)
{
}
#endif

int
get_output_handle(void)
{
	return 1;
}

int
get_ctl_handle()
{
	return 0;
}

#if defined(BEOS)

#elif defined(HAVE_BEGINTHREAD) && defined(HAVE_READ_KBD)

#elif defined(WIN32)

#else

int
get_input_handle(void)
{
	return 0;
}

#endif /* defined(HAVE_BEGINTHREAD) && defined(HAVE_READ_KBD) */


void
elinks_cfmakeraw(struct termios *t)
{
#ifdef HAVE_CFMAKERAW
	cfmakeraw(t);
#ifdef VMIN
	t->c_cc[VMIN] = 1; /* cfmakeraw() is broken on AIX --mikulas */
#endif
#else
	t->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	t->c_oflag &= ~OPOST;
	t->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	t->c_cflag &= ~(CSIZE|PARENB);
	t->c_cflag |= CS8;
	t->c_cc[VMIN] = 1;
	t->c_cc[VTIME] = 0;
#endif
}

#if defined(USE_GPM) && defined(USE_MOUSE)

struct gpm_mouse_spec {
	int h;
	void (*fn)(void *, unsigned char *, int);
	void *data;
};

static void
gpm_mouse_in(struct gpm_mouse_spec *gms)
{
	Gpm_Event gev;
	struct term_event ev;

	if (Gpm_GetEvent(&gev) <= 0) {
		set_handlers(gms->h, NULL, NULL, NULL, NULL);
		return;
	}

	ev.ev = EV_MOUSE;
	ev.x = gev.x - 1;
	ev.y = gev.y - 1;
	if (gev.buttons & GPM_B_LEFT)
		ev.b = B_LEFT;
	else if (gev.buttons & GPM_B_MIDDLE)
		ev.b = B_MIDDLE;
	else if (gev.buttons & GPM_B_RIGHT)
		ev.b = B_RIGHT;
	else
		return;

	if (gev.type & GPM_DOWN)
		ev.b |= B_DOWN;
	else if (gev.type & GPM_UP)
		ev.b |= B_UP;
	else if (gev.type & GPM_DRAG)
		ev.b |= B_DRAG;
	else
		return;

	gms->fn(gms->data, (char *)&ev, sizeof(struct term_event));
}

int
init_mouse(int cons)
{
	Gpm_Connect conn;

	conn.eventMask = ~GPM_MOVE;
	conn.defaultMask = GPM_MOVE;
	conn.minMod = 0;
	conn.maxMod = 0;

	return Gpm_Open(&conn, cons);
}

int
done_mouse(void)
{
	return Gpm_Close();
}

void *
handle_mouse(int cons, void (*fn)(void *, unsigned char *, int),
	     void *data)
{
	int h;
	struct gpm_mouse_spec *gms;

	h = init_mouse(cons, 0);
	if (h < 0) return NULL;

	gms = mem_alloc(sizeof(struct gpm_mouse_spec));
	if (!gms) return NULL;
	gms->h = h;
	gms->fn = fn;
	gms->data = data;
	set_handlers(h, (void (*)(void *))gpm_mouse_in, NULL, NULL, gms);

	return gms;
}

void
unhandle_mouse(void *h)
{
	struct gpm_mouse_spec *gms = h;

	if (!gms) return;

	set_handlers(gms->h, NULL, NULL, NULL, NULL);
	mem_free(gms);
	done_mouse();
}

#elif !defined(USING_OS2_MOUSE)

void *
handle_mouse(int cons, void (*fn)(void *, unsigned char *, int),
	     void *data)
{
	return NULL;
}

void
unhandle_mouse(void *data)
{
}

#endif /* #ifdef USE_GPM */

/* Create a bitmask consisting from system-independent envirnoment modifiers.
 * This is then complemented by system-specific modifiers in an appropriate
 * get_system_env() routine. */
static int
get_common_env(void)
{
	int env = 0;

	/* TODO: This should be named uniformly ;-). --pasky */
	if (is_xterm()) env |= ENV_XWIN;
	if (is_twterm()) env |= ENV_TWIN;
	if (getenv("STY")) env |= ENV_SCREEN;

	/* ENV_CONSOLE is always set now and indicates that we are working w/ a
	 * displaying-capable character-adressed terminal. Sounds purely
	 * theoretically now, but it already makes some things easier and it
	 * could give us interesting opportunities later (after graphical
	 * frontends will be introduced, when working in some mysterious daemon
	 * mode or who knows what ;). --pasky */
	env |= ENV_CONSOLE;

	return env;
}

#if defined(OS2)

#elif defined(BEOS)

#elif defined(WIN32)

#else

int
get_system_env(void)
{
	return get_common_env();
}

#endif

static void
exec_new_elinks(struct terminal *term, unsigned char *xterm,
		unsigned char *exe_name, unsigned char *param)
{
	unsigned char *str = straconcat(xterm, " ", exe_name, " ", param, NULL);

	if (!str) return;
	exec_on_terminal(term, str, "", 2);
	mem_free(str);
}

static void
open_in_new_twterm(struct terminal *term, unsigned char *exe_name,
		   unsigned char *param)
{
	unsigned char *twterm = getenv("ELINKS_TWTERM");

	if (!twterm) twterm = getenv("LINKS_TWTERM");
	if (!twterm) twterm = DEFAULT_TWTERM_CMD;
	exec_new_elinks(term, twterm, exe_name, param);
}

static void
open_in_new_xterm(struct terminal *term, unsigned char *exe_name,
		  unsigned char *param)
{
	unsigned char *xterm = getenv("ELINKS_XTERM");

	if (!xterm) xterm = getenv("LINKS_XTERM");
	if (!xterm) xterm = DEFAULT_XTERM_CMD;
	exec_new_elinks(term, xterm, exe_name, param);
}

static void
open_in_new_screen(struct terminal *term, unsigned char *exe_name,
		   unsigned char *param)
{
	exec_new_elinks(term, DEFAULT_SCREEN_CMD, exe_name, param);
}

#ifdef OS2
void open_in_new_vio(struct terminal *term, unsigned char *exe_name,
		unsigned char *param);
void open_in_new_fullscreen(struct terminal *term, unsigned char *exe_name,
		       unsigned char *param);
#endif

#ifdef WIN32
void
open_in_new_win32(struct terminal *term, unsigned char *exe_name,
		  unsigned char *param);
#endif

#ifdef BEOS
void open_in_new_be(struct terminal *term, unsigned char *exe_name,
		    unsigned char *param);
#endif

struct {
	enum term_env_type env;
	void (*fn)(struct terminal *term, unsigned char *, unsigned char *);
	unsigned char *text;
} oinw[] = {
	{ENV_XWIN, open_in_new_xterm, N_("~Xterm")},
	{ENV_TWIN, open_in_new_twterm, N_("T~wterm")},
	{ENV_SCREEN, open_in_new_screen, N_("~Screen")},
#ifdef OS2
	{ENV_OS2VIO, open_in_new_vio, N_("~Window")},
	{ENV_OS2VIO, open_in_new_fullscreen, N_("~Full screen")},
#endif
#ifdef WIN32
	{ENV_WIN32, open_in_new_win32, N_("~Window")},
#endif
#ifdef BEOS
	{ENV_BE, open_in_new_be, N_("~BeOS terminal")},
#endif
	{0, NULL, NULL}
};

struct open_in_new *
get_open_in_new(int environment)
{
	int i;
	struct open_in_new *oin = NULL;
	int noin = 0;

	for (i = 0; oinw[i].env; i++) {
		struct open_in_new *x;

		if (!(environment & oinw[i].env))
			continue;

		x = mem_realloc(oin, (noin + 2) * sizeof(struct open_in_new));
		if (!x) continue;

		oin = x;
		oin[noin].text = oinw[i].text;
		oin[noin].fn = oinw[i].fn;
		noin++;
		oin[noin].text = NULL;
		oin[noin].fn = NULL;
	}

	return oin;
}

/* Returns:
 * 0 if it is impossible to open anything in anything new
 * 1 if there is one possible object capable of being spawn
 * >1 if there is >1 such available objects (it may not be the actual number of
 *    them, though) */
int
can_open_in_new(struct terminal *term)
{
	struct open_in_new *oin = get_open_in_new(term->environment);

	if (!oin) return 0;
	if (!oin[1].text) {
		mem_free(oin);
		return 1;
	}
	mem_free(oin);
	return 2;
}

int
can_resize_window(int environment)
{
	return !!(environment & ENV_OS2VIO);
}

#ifndef OS2
int
can_open_os_shell(int environment)
{
	if (environment & ENV_XWIN) return 0;
	return 1;
}

void
set_highpri(void)
{
}
#endif

unsigned char *
get_system_str(int xwin)
{
	unsigned char *system_str = NULL;

	if (xwin)
		system_str = stracpy(SYSTEM_STR "-xwin");
	else
		system_str = stracpy(SYSTEM_STR);

	return system_str;
}
