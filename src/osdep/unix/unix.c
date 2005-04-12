/* UNIX system-specific routines. */
/* $Id: unix.c,v 1.24 2005/04/12 17:50:02 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "osdep/system.h"

#if defined(CONFIG_GPM) && defined(HAVE_GPM_H)
#include <gpm.h>
#endif

#include "elinks.h"

#include "lowlevel/select.h"
#include "osdep/unix/unix.h"
#include "osdep/osdep.h"
#include "terminal/event.h"
#include "terminal/mouse.h"
#include "util/memory.h"


#if defined(CONFIG_GPM) && defined(CONFIG_MOUSE)

struct gpm_mouse_spec {
	int h;
	int cons;
	void (*fn)(void *, unsigned char *, int);
	void *data;
};

static void
gpm_mouse_in(struct gpm_mouse_spec *gms)
{
	Gpm_Event gev;
	struct term_event ev;

	if (Gpm_GetEvent(&gev) <= 0) {
		clear_handlers(gms->h);
		return;
	}

	ev.ev = EVENT_MOUSE;
	ev.info.mouse.x = int_max(gev.x - 1, 0);
	ev.info.mouse.y = int_max(gev.y - 1, 0);

	if (gev.buttons & GPM_B_LEFT)
		ev.info.mouse.button = B_LEFT;
	else if (gev.buttons & GPM_B_MIDDLE)
		ev.info.mouse.button = B_MIDDLE;
	else if (gev.buttons & GPM_B_RIGHT)
		ev.info.mouse.button = B_RIGHT;
	else
		return;

	if (gev.type & GPM_DOWN)
		ev.info.mouse.button |= B_DOWN;
	else if (gev.type & GPM_UP)
		ev.info.mouse.button |= B_UP;
	else if (gev.type & GPM_DRAG)
		ev.info.mouse.button |= B_DRAG;
	else
		return;

	gms->fn(gms->data, (char *) &ev, sizeof(ev));
}

int
init_mouse(int cons, int suspend)
{
	Gpm_Connect conn;

	conn.eventMask = suspend ? 0 : ~GPM_MOVE;
	conn.defaultMask = suspend ? ~0 : GPM_MOVE;
	conn.minMod = suspend ? ~0 : 0;
	conn.maxMod = suspend ? ~0 : 0;

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

	gms = mem_alloc(sizeof(*gms));
	if (!gms) return NULL;
	gms->h = h;
	gms->cons = cons;
	gms->fn = fn;
	gms->data = data;
	set_handlers(h, (select_handler_T) gpm_mouse_in, NULL, NULL, gms);

	return gms;
}

void
unhandle_mouse(void *h)
{
	struct gpm_mouse_spec *gms = h;

	if (!gms) return;

	clear_handlers(gms->h);
	mem_free(gms);
	done_mouse();
}

void
suspend_mouse(void *h)
{
	struct gpm_mouse_spec *gms = h;

	if (!gms) return;

	gms->h = init_mouse(gms->cons, 1);
	if (gms->h < 0) return;

	clear_handlers(gms->h);
}

void
resume_mouse(void *h)
{
	struct gpm_mouse_spec *gms = h;

	if (!gms) return;

	gms->h = init_mouse(gms->cons, 0);
	if (gms->h < 0) return;

	set_handlers(gms->h, (select_handler_T) gpm_mouse_in, NULL, NULL, gms);
}

#endif
