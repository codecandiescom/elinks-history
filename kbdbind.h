/* $Id: kbdbind.h,v 1.2 2002/03/16 23:02:36 pasky Exp $ */

#ifndef EL__KBDBIND_H
#define EL__KBDBIND_H

#include "default.h"

enum keymap {
	KM_MAIN,
	KM_EDIT,
	KM_MENU,
	KM_MAX,
};

/* Note: if you add anything here, please keep it in alphabetical order,
 * and also update the table in parse_act() in kbdbind.c.  */
enum keyact {
	ACT_ADD_BOOKMARK,
	ACT_AUTO_COMPLETE,
	ACT_BACK,
	ACT_BACKSPACE,
	ACT_BOOKMARK_MANAGER,
	ACT_COOKIES_LOAD,
	ACT_COPY_CLIPBOARD,
	ACT_CUT_CLIPBOARD,
	ACT_DELETE,
	ACT_DOCUMENT_INFO,
	ACT_DOWN,
	ACT_DOWNLOAD,
	ACT_EDIT,
	ACT_END,
	ACT_ENTER,
	ACT_FILE_MENU,
	ACT_FIND_NEXT,
	ACT_FIND_NEXT_BACK,
	ACT_GOTO_URL,
	ACT_GOTO_URL_CURRENT,
	ACT_GOTO_URL_CURRENT_LINK,
	ACT_HEADER_INFO,
	ACT_HOME,
	ACT_KILL_TO_BOL,
	ACT_KILL_TO_EOL,
	ACT_LEFT,
	ACT_LUA_CONSOLE,
	ACT_LUA_FUNCTION,
	ACT_MENU,
	ACT_NEXT_FRAME,
	ACT_OPEN_NEW_WINDOW,
	ACT_OPEN_LINK_IN_NEW_WINDOW,
	ACT_PAGE_DOWN,
	ACT_PAGE_UP,
	ACT_PASTE_CLIPBOARD,
	ACT_PREVIOUS_FRAME,
	ACT_QUIT,
	ACT_REALLYQUIT,
	ACT_RELOAD,
	ACT_RIGHT,
	ACT_SCROLL_DOWN,
	ACT_SCROLL_LEFT,
	ACT_SCROLL_RIGHT,
	ACT_SCROLL_UP,
	ACT_SEARCH,
	ACT_SEARCH_BACK,
	ACT_TOGGLE_DISPLAY_IMAGES,
	ACT_TOGGLE_DISPLAY_TABLES,
	ACT_TOGGLE_HTML_PLAIN,
	ACT_UNBACK,
	ACT_UP,
	ACT_VIEW_IMAGE,
	ACT_ZOOM_FRAME
};

void init_keymaps();
void free_keymaps();

long parse_key(unsigned char *);
int kbd_action(enum keymap, struct event *, int *);

unsigned char *bind_rd(struct option *, unsigned char *);
unsigned char *unbind_rd(struct option *, unsigned char *);

#ifdef HAVE_LUA
unsigned char *bind_lua_func(unsigned char *, unsigned char *, int);
#endif

#endif
