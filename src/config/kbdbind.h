/* $Id: kbdbind.h,v 1.41 2003/07/07 20:13:25 jonas Exp $ */

#ifndef EL__CONFIG_KBDBIND_H
#define EL__CONFIG_KBDBIND_H

/* #include "bfu/listbox.h" */
struct listbox_item;

#include "config/options.h"
#include "terminal/terminal.h"

enum keymap {
	KM_MAIN,
	KM_EDIT,
	KM_MENU,
	KM_MAX,
};

/* Note: if you add anything here, please keep it in alphabetical order,
 * and also update the table action_table[] in kbdbind.c.  */
enum keyact {
	ACT_NONE,
	ACT_ABORT_CONNECTION,
	ACT_ADD_BOOKMARK,
	ACT_ADD_BOOKMARK_LINK,
	ACT_AUTO_COMPLETE,
	ACT_AUTO_COMPLETE_UNAMBIGUOUS,
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
	ACT_DOWNLOAD_IMAGE,
	ACT_EDIT,
	ACT_END,
	ACT_ENTER,
	ACT_ENTER_RELOAD,
	ACT_FILE_MENU,
	ACT_FIND_NEXT,
	ACT_FIND_NEXT_BACK,
	ACT_FORGET_CREDENTIALS,
	ACT_GOTO_URL,
	ACT_GOTO_URL_CURRENT,
	ACT_GOTO_URL_CURRENT_LINK,
	ACT_GOTO_URL_HOME,
	ACT_HEADER_INFO,
	ACT_HISTORY_MANAGER,
	ACT_HOME,
	ACT_KILL_TO_BOL,
	ACT_KILL_TO_EOL,
	ACT_KEYBINDING_MANAGER,
	ACT_LEFT,
	ACT_LINK_MENU,
	ACT_JUMP_TO_LINK,
	ACT_LUA_CONSOLE,
	ACT_LUA_FUNCTION,
	ACT_MENU,
	ACT_NEXT_FRAME,
	ACT_OPEN_NEW_WINDOW,
	ACT_OPEN_LINK_IN_NEW_WINDOW,
	ACT_OPTIONS_MANAGER,
	ACT_PAGE_DOWN,
	ACT_PAGE_UP,
	ACT_PASTE_CLIPBOARD,
	ACT_PREVIOUS_FRAME,
	ACT_QUIT,
	ACT_REALLY_QUIT,
	ACT_RELOAD,
	ACT_RESUME_DOWNLOAD,
	ACT_RIGHT,
	ACT_SAVE_FORMATTED,
	ACT_SCROLL_DOWN,
	ACT_SCROLL_LEFT,
	ACT_SCROLL_RIGHT,
	ACT_SCROLL_UP,
	ACT_SEARCH,
	ACT_SEARCH_BACK,
	ACT_TAB_CLOSE,
	ACT_TAB_NEXT,
	ACT_TAB_PREV,
	ACT_TOGGLE_DISPLAY_IMAGES,
	ACT_TOGGLE_DISPLAY_TABLES,
	ACT_TOGGLE_HTML_PLAIN,
	ACT_TOGGLE_NUMBERED_LINKS,
	ACT_UNBACK,
	ACT_UP,
	ACT_VIEW_IMAGE,
	ACT_ZOOM_FRAME,
	KEYACTS,
};

enum kbdbind_flags {
	KBDB_WATERMARK = 1,
	KBDB_TOUCHED = 2,
};

struct keybinding {
	LIST_HEAD(struct keybinding);
	enum keymap keymap;
	enum keyact action;
	long key;
	long meta;
	int func_ref;
	int flags;
	struct listbox_item *box_item;
};

extern struct list_head kbdbind_box_items;
extern struct list_head kbdbind_boxes;


void init_keymaps(void);
void free_keymaps(void);

void add_keybinding(enum keymap km, int action, long key, long meta, int func_ref);
void delete_keybinding(enum keymap km, long key, long meta);
void free_keybinding(struct keybinding *);

long read_key(unsigned char *);
unsigned char *write_action(int);
unsigned char *write_keymap(enum keymap);

void toggle_display_action_listboxes(void);

int parse_keystroke(unsigned char *, long *, long *);
void make_keystroke(unsigned char **, int *, long, long);

int kbd_action(enum keymap, struct event *, int *);
struct keybinding *kbd_ev_lookup(enum keymap, long, long, int *);
struct keybinding *kbd_nm_lookup(enum keymap, unsigned char *, int *);

int bind_do(unsigned char *, unsigned char *, unsigned char *);
unsigned char *bind_act(unsigned char *, unsigned char *);
void bind_config_string(unsigned char **, int *);

#ifdef HAVE_LUA
unsigned char *bind_lua_func(unsigned char *, unsigned char *, int);
#endif

#endif
