/* UNIX system-specific routines. */
/* $Id: unix.c,v 1.5 2003/10/27 03:29:08 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "osdep/system.h"

#ifdef UNIX

#ifdef GPM_MOUSE
#include <gpm.h>
#endif

#include "elinks.h"

#include "util/memory.h"


#if defined(GPM_MOUSE) && defined(USE_MOUSE)

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

#endif

#endif
