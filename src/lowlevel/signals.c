/* Signals handling. */
/* $Id: signals.c,v 1.4 2003/05/25 09:41:44 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_SIGNAL_H
#include <sys/signal.h>
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "main.h"
/* This does not deserve to survive. Gotta be moved... somewhere else.
 * 'nuff said. --pasky */
#include "dialogs/menu.h"
#include "lowlevel/select.h"
#include "lowlevel/signals.h"
#include "terminal/kbd.c"
#include "util/error.h"
#include "version.h"


static void unhandle_basic_signals(struct terminal *term);

static void
sig_terminate(struct terminal *t)
{
	unhandle_basic_signals(t);
	terminate = 1;
	retval = RET_SIGNAL;
}

static void
sig_intr(struct terminal *t)
{
	unhandle_basic_signals(t);

	if (!t)
		terminate = 1;
	else
		exit_prog(t, NULL, NULL);
}

void
sig_ctrl_c(struct terminal *t)
{
	if (!is_blocked()) kbd_ctrl_c();
}

static void
sig_ign(void *x)
{
}

static void
sig_tstp(struct terminal *t)
{
#ifdef SIGSTOP
	int pid = getpid();

	block_itrm(0);
#if defined (SIGCONT) && defined(SIGTTOU)
	if (!fork()) {
		sleep(1);
		kill(pid, SIGCONT);
		exit(0);
	}
#endif
	raise(SIGSTOP);
#endif
}

static void
sig_cont(struct terminal *t)
{
	if (!unblock_itrm(0)) {
		/* redraw_terminal_cls(t); */
		resize_terminal();
	} /* else {
		register_bottom_half(raise, SIGSTOP);
	} */
}

#ifdef BACKTRACE
static void
sig_segv(struct terminal *t)
{
	/* Get some attention. */
	fputs("\a", stderr); fflush(stderr); sleep(1); fputs("\a\n", stderr);

	/* Rant. */
	fputs(	"ELinks crashed. That shouldn't happen. Please report this incident to\n"
		"developers. Preferrably please include information about what probably\n"
		"triggered this and the listout below. Note that it does NOT supercede the gdb\n"
		"output, which is way more useful for developers. If you would like to help to\n"
		"debug the problem you just uncovered, please keep the core you just got and\n"
		"send the developers output of 'bt' command entered inside of gdb (which you run\n"
		"as gdb elinks core). Thanks a lot for your cooperation!\n\n", stderr);

	/* version information */
	fputs(full_static_version, stderr);
	fputs("\n\n", stderr);

	/* Backtrace. */
	dump_backtrace(stderr, 1);

	/* TODO: Perhaps offer launching of gdb? Or trying to continue w/
	 * program execution? --pasky */

	/* The fastest way OUT! */
	abort();
}
#endif


void
handle_basic_signals(struct terminal *term)
{
	install_signal_handler(SIGHUP, (void (*)(void *))sig_intr, term, 0);
	install_signal_handler(SIGINT, (void (*)(void *))sig_ctrl_c, term, 0);
	install_signal_handler(SIGTERM, (void (*)(void *))sig_terminate, term, 0);
#ifdef SIGTSTP
	install_signal_handler(SIGTSTP, (void (*)(void *))sig_tstp, term, 0);
#endif
#ifdef SIGTTIN
	install_signal_handler(SIGTTIN, (void (*)(void *))sig_tstp, term, 0);
#endif
#ifdef SIGTTOU
	install_signal_handler(SIGTTOU, (void (*)(void *))sig_ign, term, 0);
#endif
#ifdef SIGCONT
	install_signal_handler(SIGCONT, (void (*)(void *))sig_cont, term, 0);
#endif
#ifdef BACKTRACE
	install_signal_handler(SIGSEGV, (void (*)(void *))sig_segv, term, 1);
#endif
}

#if 0
void handle_slave_signals(struct terminal *term)
{
	install_signal_handler(SIGHUP, (void (*)(void *))sig_terminate, term, 0);
	install_signal_handler(SIGINT, (void (*)(void *))sig_terminate, term, 0);
	install_signal_handler(SIGTERM, (void (*)(void *))sig_terminate, term, 0);
#ifdef SIGTSTP
	install_signal_handler(SIGTSTP, (void (*)(void *))sig_tstp, term, 0);
#endif
#ifdef SIGTTIN
	install_signal_handler(SIGTTIN, (void (*)(void *))sig_tstp, term, 0);
#endif
#ifdef SIGTTOU
	install_signal_handler(SIGTTOU, (void (*)(void *))sig_ign, term, 0);
#endif
#ifdef SIGCONT
	install_signal_handler(SIGCONT, (void (*)(void *))sig_cont, term, 0);
#endif
}
#endif


void
unhandle_terminal_signals(struct terminal *term)
{
	install_signal_handler(SIGHUP, NULL, NULL, 0);
	install_signal_handler(SIGINT, NULL, NULL, 0);
#ifdef SIGTSTP
	install_signal_handler(SIGTSTP, NULL, NULL, 0);
#endif
#ifdef SIGTTIN
	install_signal_handler(SIGTTIN, NULL, NULL, 0);
#endif
#ifdef SIGTTOU
	install_signal_handler(SIGTTOU, NULL, NULL, 0);
#endif
#ifdef SIGCONT
	install_signal_handler(SIGCONT, NULL, NULL, 0);
#endif
#ifdef BACKTRACE
	install_signal_handler(SIGSEGV, NULL, NULL, 0);
#endif
}

static void
unhandle_basic_signals(struct terminal *term)
{
	install_signal_handler(SIGHUP, NULL, NULL, 0);
	install_signal_handler(SIGINT, NULL, NULL, 0);
	install_signal_handler(SIGTERM, NULL, NULL, 0);
#ifdef SIGTSTP
	install_signal_handler(SIGTSTP, NULL, NULL, 0);
#endif
#ifdef SIGTTIN
	install_signal_handler(SIGTTIN, NULL, NULL, 0);
#endif
#ifdef SIGTTOU
	install_signal_handler(SIGTTOU, NULL, NULL, 0);
#endif
#ifdef SIGCONT
	install_signal_handler(SIGCONT, NULL, NULL, 0);
#endif
#ifdef BACKTRACE
	install_signal_handler(SIGSEGV, NULL, NULL, 0);
#endif
}


static int signal_mask[NUM_SIGNALS];
static struct signal_handler signal_handlers[NUM_SIGNALS];
int critical_section = 0;

static void check_for_select_race(void);

/* TODO: In order to gain better portability, we should use signal() instead.
 * Highest care should be given to careful watching of which signals are
 * blocked and which aren't then, though. --pasky */

static void
got_signal(int sig)
{
	if (sig >= NUM_SIGNALS || sig < 0) {
		error("ERROR: bad signal number: %d", sig);
		return;
	}

	if (!signal_handlers[sig].fn) return;

	if (signal_handlers[sig].critical) {
		signal_handlers[sig].fn(signal_handlers[sig].data);
		return;
	}

	signal_mask[sig] = 1;
	check_for_select_race();
}

void
install_signal_handler(int sig, void (*fn)(void *), void *data, int critical)
{
	struct sigaction sa;

	if (sig >= NUM_SIGNALS || sig < 0) {
		internal("bad signal number: %d", sig);
		return;
	}

	memset(&sa, 0, sizeof(sa));
	if (!fn)
		sa.sa_handler = SIG_IGN;
	else
		sa.sa_handler = got_signal;

	sigfillset(&sa.sa_mask);
	/*sa.sa_flags = SA_RESTART;*/
	if (!fn) sigaction(sig, &sa, NULL);
	signal_handlers[sig].fn = fn;
	signal_handlers[sig].data = data;
	signal_handlers[sig].critical = critical;
	if (fn) sigaction(sig, &sa, NULL);
}

static int pending_alarm = 0;

static void
alarm_handler(void *x)
{
	pending_alarm = 0;
	check_for_select_race();
}

static void
check_for_select_race(void)
{
	if (critical_section) {
#ifdef SIGALRM
		install_signal_handler(SIGALRM, alarm_handler, NULL, 1);
#endif
		pending_alarm = 1;
#ifdef HAVE_ALARM
		/*alarm(1);*/
#endif
	}
}

void
uninstall_alarm(void)
{
	pending_alarm = 0;
#ifdef HAVE_ALARM
	alarm(0);
#endif
}


static void
sigchld(void *p)
{
#ifdef WNOHANG
	while ((int) waitpid(-1, NULL, WNOHANG) > 0);
#else
	wait(NULL);
#endif
}

void
set_sigcld(void)
{
	install_signal_handler(SIGCHLD, sigchld, NULL, 1);
}

void
clear_signal_mask_and_handlers(void)
{
	memset(signal_mask, 0, sizeof(signal_mask));
	memset(signal_handlers, 0, sizeof(signal_handlers));
}

int
check_signals(void)
{
	int i, r = 0;

	for (i = 0; i < NUM_SIGNALS; i++)
		if (signal_mask[i]) {
			signal_mask[i] = 0;
			if (signal_handlers[i].fn)
				signal_handlers[i].fn(signal_handlers[i].data);
			check_bottom_halves();
			r = 1;
		}

	return r;
}
