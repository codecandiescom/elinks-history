/* Signals handling. */
/* $Id: signals.c,v 1.26 2004/11/08 19:37:52 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
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
#include "lowlevel/select.h"
#include "lowlevel/signals.h"
#include "modules/version.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "util/error.h"


static void unhandle_basic_signals(struct terminal *term);

static void
sig_terminate(struct terminal *term)
{
	unhandle_basic_signals(term);
	terminate = 1;
	retval = RET_SIGNAL;
}

static void
sig_intr(struct terminal *term)
{
	unhandle_basic_signals(term);

	if (!term)
		terminate = 1;
	else
		register_bottom_half((void (*)(void *)) destroy_terminal, term);
}

void
sig_ctrl_c(struct terminal *term)
{
	if (!is_blocked()) kbd_ctrl_c();
}

static void
sig_ign(void *x)
{
}

static void
sig_tstp(struct terminal *term)
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
sig_cont(struct terminal *term)
{
	if (!unblock_itrm(0)) resize_terminal();
}

#ifdef CONFIG_BACKTRACE
static void
sig_segv(struct terminal *term)
{
	/* Get some attention. */
	fputs("\a", stderr); fflush(stderr); sleep(1); fputs("\a\n", stderr);

	/* Rant. */
	fputs(	"ELinks crashed. That shouldn't happen. Please report this incident to\n"
		"developers. If you would like to help to debug the problem you just\n"
		"uncovered, please keep the core you just got and send the developers\n"
		"output of 'bt' command entered inside of gdb (which you run as:\n"
		"gdb elinks core). Thanks a lot for your cooperation!\n\n", stderr);

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
	install_signal_handler(SIGHUP, (void (*)(void *)) sig_intr, term, 0);
	install_signal_handler(SIGINT, (void (*)(void *)) sig_ctrl_c, term, 0);
	install_signal_handler(SIGTERM, (void (*)(void *)) sig_terminate, term, 0);
#ifdef SIGTSTP
	install_signal_handler(SIGTSTP, (void (*)(void *)) sig_tstp, term, 0);
#endif
#ifdef SIGTTIN
	install_signal_handler(SIGTTIN, (void (*)(void *)) sig_tstp, term, 0);
#endif
#ifdef SIGTTOU
	install_signal_handler(SIGTTOU, (void (*)(void *)) sig_ign, term, 0);
#endif
#ifdef SIGCONT
	install_signal_handler(SIGCONT, (void (*)(void *)) sig_cont, term, 0);
#endif
#ifdef CONFIG_BACKTRACE
	install_signal_handler(SIGSEGV, (void (*)(void *)) sig_segv, term, 1);
#endif
}

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
#ifdef CONFIG_BACKTRACE
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
#ifdef CONFIG_BACKTRACE
	install_signal_handler(SIGSEGV, NULL, NULL, 0);
#endif
}

struct signal_info {
	void (*handler)(void *);
	void *data;
	int critical;
	int mask;
};

static struct signal_info signal_info[NUM_SIGNALS];
volatile int critical_section = 0;

static void check_for_select_race(void);

/* TODO: In order to gain better portability, we should use signal() instead.
 * Highest care should be given to careful watching of which signals are
 * blocked and which aren't then, though. --pasky */

static void
got_signal(int sig)
{
	struct signal_info *s;
	int saved_errno = errno;

	if (sig >= NUM_SIGNALS || sig < 0) {
		/* Signal handler - we have no good way how to tell this the
		 * user. She won't care anyway, tho'. */
		return;
	}

	s = &signal_info[sig];

	if (!s->handler) return;

	if (s->critical) {
		s->handler(s->data);
		errno = saved_errno;
		return;
	}

	s->mask = 1;
	check_for_select_race();

	errno = saved_errno;
}

void
install_signal_handler(int sig, void (*fn)(void *), void *data, int critical)
{
	struct sigaction sa;

	/* Yes, assertm() in signal handler is totally unsafe and depends just
	 * on good luck. But hey, assert()ions are never triggered ;-). */
	assertm(sig >= 0 && sig < NUM_SIGNALS, "bad signal number: %d", sig);
	if_assert_failed return;

	/* AIX has problem with empty static initializers. */
	memset(&sa, 0, sizeof(sa));

	if (!fn)
		sa.sa_handler = SIG_IGN;
	else
		sa.sa_handler = got_signal;

	sigfillset(&sa.sa_mask);
	if (!fn) sigaction(sig, &sa, NULL);
	signal_info[sig].handler = fn;
	signal_info[sig].data = data;
	signal_info[sig].critical = critical;
	if (fn) sigaction(sig, &sa, NULL);
}

static volatile int pending_alarm = 0;

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
		alarm(1);
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
	memset(signal_info, 0, sizeof(signal_info));
}

int
check_signals(void)
{
	int i, r = 0;

	for (i = 0; i < NUM_SIGNALS; i++) {
		struct signal_info *s = &signal_info[i];

		if (!s->mask) continue;

		s->mask = 0;
		if (s->handler)
			s->handler(s->data);
		check_bottom_halves();
		r = 1;
	}

	return r;
}
