/* Keybinding implementation */
/* $Id: kbdbind.c,v 1.94 2003/11/14 15:35:28 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <string.h>

#include "elinks.h"

#include "bfu/listbox.h"
#include "config/conf.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "sched/event.h"
#include "terminal/kbd.h"
#include "util/memory.h"
#include "util/string.h"


/* Fix namespace clash on MacOS. */
#define table table_elinks

INIT_LIST_HEAD(kbdbind_box_items);
INIT_LIST_HEAD(kbdbind_boxes);

static struct listbox_item *keyact_box_items[KEYACTS];
static struct list_head keymaps[KM_MAX];

static int read_action(unsigned char *);
static void add_default_keybindings(void);
static void init_action_listboxes(void);
static void free_action_listboxes(void);


void
add_keybinding(enum keymap km, int action, long key, long meta, int func_ref)
{
	struct keybinding *kb;

	delete_keybinding(km, key, meta);

	kb = mem_alloc(sizeof(struct keybinding));
	if (kb) {
		struct listbox_item *keymap;
		struct string keystroke;

		kb->keymap = km;
		kb->action = action;
		kb->key = key;
		kb->meta = meta;
		kb->func_ref = func_ref;
		kb->flags &= ~KBDB_WATERMARK;
		add_to_list(keymaps[km], kb);

		if (action == ACT_NONE) {
			/* We don't want such a listbox_item, do we? */
			kb->box_item = NULL;
			return; /* Or goto. */
		}

		if (!init_string(&keystroke)) return;

		make_keystroke(&keystroke, key, meta, 0);
		kb->box_item = mem_calloc(1, sizeof(struct listbox_item)
					  + keystroke.length + 1);
		if (!kb->box_item) {
			done_string(&keystroke);
			return; /* Or just goto after end of this if block. */
		}
		kb->box_item->text = ((unsigned char *) kb->box_item
					+ sizeof(struct listbox_item));
		strcpy(kb->box_item->text, keystroke.source);
		done_string(&keystroke);

		if (!keyact_box_items[action]) {
boom:
			mem_free(kb->box_item);
			kb->box_item = NULL;
			return; /* Or goto ;-). */
		}
		for (keymap = keyact_box_items[action]->child.next;
		     keymap != (struct listbox_item *) &keyact_box_items[action]->child && km;
		     km--)
			keymap = keymap->next;
		if (keymap == (struct listbox_item *) &keyact_box_items[action]->child)
			goto boom;

		add_to_list(keymap->child, kb->box_item);
		kb->box_item->root = keymap;
		init_list(kb->box_item->child);
		kb->box_item->visible = 1;
		kb->box_item->translated = 1;
		kb->box_item->udata = kb;
		kb->box_item->type = BI_LEAF;
		kb->box_item->depth = keymap->depth + 1;
		kb->box_item->box = &kbdbind_boxes;
	}
}

void
free_keybinding(struct keybinding *kb)
{
	if (kb->box_item) {
		del_from_list(kb->box_item);
		mem_free(kb->box_item);
	}
#ifdef HAVE_SCRIPTING
/* TODO: unref function must be implemented. */
/*	if (kb->func_ref != EVENT_NONE)
		scripting_unref(kb->func_ref); */
#endif
	del_from_list(kb);
	mem_free(kb);
}

void
delete_keybinding(enum keymap km, long key, long meta)
{
	struct keybinding *kb;

	foreach (kb, keymaps[km]) {
		if (kb->key != key || kb->meta != meta)
			continue;

		free_keybinding(kb);
		break;
	}
}


void
init_keymaps(void)
{
    	enum keymap i;

	for (i = 0; i < KM_MAX; i++)
		init_list(keymaps[i]);

	init_action_listboxes();
	add_default_keybindings();
}

void
free_keymaps(void)
{
	enum keymap i;

	free_action_listboxes();

	for (i = 0; i < KM_MAX; i++)
		free_list(keymaps[i]);
}


int
kbd_action(enum keymap kmap, struct term_event *ev, int *func_ref)
{
	struct keybinding *kb;

	if (ev->ev != EV_KBD) return -1;

	kb = kbd_ev_lookup(kmap, ev->x, ev->y, func_ref);
	return kb ? kb->action : -1;
}

struct keybinding *
kbd_ev_lookup(enum keymap kmap, long key, long meta, int *func_ref)
{
	struct keybinding *kb;

	foreach (kb, keymaps[kmap]) {
		if (key != kb->key || meta != kb->meta)
			continue;

		if (kb->action == ACT_SCRIPTING_FUNCTION && func_ref)
			*func_ref = kb->func_ref;

		return kb;
	}

	return NULL;
}

struct keybinding *
kbd_nm_lookup(enum keymap kmap, unsigned char *name, int *func_ref)
{
	struct keybinding *kb;
	enum keyact act = read_action(name);

	if (act < 0) return NULL;

	foreach (kb, keymaps[kmap]) {
		if (act != kb->action)
			continue;

		if (kb->action == ACT_SCRIPTING_FUNCTION && func_ref)
			*func_ref = kb->func_ref;

		return kb;
	}

	return NULL;
}

struct keybinding *
kbd_act_lookup(enum keymap map, enum keyact action)
{
	struct keybinding *kb;

	foreach (kb, keymaps[map]) {
		if (action != kb->action)
			continue;

		return kb;
	}

	return NULL;
}

/*
 * Config file helpers.
 */

struct strtonum {
	unsigned char *str;
	long num;
	unsigned char *desc;
};

static long
strtonum(struct strtonum *table, unsigned char *str)
{
	struct strtonum *rec;

	for (rec = table; rec->str; rec++)
		if (!strcmp(rec->str, str))
			return rec->num;

	return -1;
}

static unsigned char *
numtostr(struct strtonum *table, long num)
{
	struct strtonum *rec;

	for (rec = table; rec->str; rec++)
		if (num == rec->num)
			return rec->str;

	return NULL;
}


static unsigned char *
numtodesc(struct strtonum *table, long num)
{
	struct strtonum *rec;

	for (rec = table; rec->str; rec++)
		if (num == rec->num)
			return (rec->desc) ? rec->desc : rec->str;

	return NULL;
}


static struct strtonum keymap_table[] = {
	{ "main", KM_MAIN, N_("Main mapping") },
	{ "edit", KM_EDIT, N_("Edit mapping") },
	{ "menu", KM_MENU, N_("Menu mapping") },
	{ NULL, 0, NULL }
};

static int
read_keymap(unsigned char *keymap)
{
	return strtonum(keymap_table, keymap);
}

unsigned char *
write_keymap(enum keymap keymap)
{
	return numtostr(keymap_table, keymap);
}


static struct strtonum key_table[] = {
	{ "Enter", KBD_ENTER },
	{ "Backspace", KBD_BS },
	{ "Tab", KBD_TAB },
	{ "Escape", KBD_ESC },
	{ "Left", KBD_LEFT },
	{ "Right", KBD_RIGHT },
	{ "Up", KBD_UP },
	{ "Down", KBD_DOWN },
	{ "Insert", KBD_INS },
	{ "Delete", KBD_DEL },
	{ "Home", KBD_HOME },
	{ "End", KBD_END },
	{ "PageUp", KBD_PAGE_UP },
	{ "PageDown", KBD_PAGE_DOWN },
	{ "F1", KBD_F1 },
	{ "F2", KBD_F2 },
	{ "F3", KBD_F3 },
	{ "F4", KBD_F4 },
	{ "F5", KBD_F5 },
	{ "F6", KBD_F6 },
	{ "F7", KBD_F7 },
	{ "F8", KBD_F8 },
	{ "F9", KBD_F9 },
	{ "F10", KBD_F10 },
	{ "F11", KBD_F11 },
	{ "F12", KBD_F12 },
	{ NULL, 0 }
};

long
read_key(unsigned char *key)
{
	return (key[0] && !key[1]) ? *key : strtonum(key_table, key);
}

int
parse_keystroke(unsigned char *s, long *key, long *meta)
{
	*meta = 0;
	if (!strncmp(s, "Shift-", 6)) {
		*meta |= KBD_SHIFT;
		s += 6;
	} else if (!strncmp(s, "Ctrl-", 5)) {
		*meta |= KBD_CTRL;
		s += 5;
		if (s[0] && !s[1]) s[0] = toupper(s[0]);
	} else if (!strncmp(s, "Alt-", 4)) {
		*meta |= KBD_ALT;
		s += 4;
	}

	*key = read_key(s);
	return (*key < 0) ? -1 : 0;
}

void
make_keystroke(struct string *str, long key, long meta, int escape)
{
	unsigned char key_buffer[3] = "xx";
	unsigned char *key_string = numtostr(key_table, key);

	if (meta & KBD_SHIFT)
		add_to_string(str, "Shift-");
	if (meta & KBD_CTRL)
		add_to_string(str, "Ctrl-");
	if (meta & KBD_ALT)
		add_to_string(str, "Alt-");

	if (!key_string) {
		key_string = key_buffer + 1;
		*key_string = (unsigned char) key;
		if (key == '\\' && escape) {
			*--key_string = '\\';
		}
	}

	add_to_string(str, key_string);
}

#ifndef ELINKS_SMALL
#define DACT(x) (x)
#else
#define DACT(x) (NULL)
#endif

/* Please keep this table in alphabetical order, and in sync with
 * the ACT_* constants in kbdbind.h.  */
static struct strtonum action_table[] = {
	{ "none", ACT_NONE, NULL },
	{ "abort-connection", ACT_ABORT_CONNECTION, DACT(N_("Abort connection")) },
	{ "add-bookmark", ACT_ADD_BOOKMARK, DACT(N_("Add a new bookmark")) },
	{ "add-bookmark-link", ACT_ADD_BOOKMARK_LINK, DACT(N_("Add a new bookmark using current link")) },
	{ "auto-complete", ACT_AUTO_COMPLETE, DACT(N_("Attempt to auto-complete the input")) },
	{ "auto-complete-unambiguous", ACT_AUTO_COMPLETE_UNAMBIGUOUS, DACT(N_("Attempt to unambiguously auto-complete the input")) },
	{ "back", ACT_BACK, DACT(N_("Return to the previous document in history")) },
	{ "backspace", ACT_BACKSPACE, DACT(N_("Delete character in front of the cursor")) },
	{ "bookmark-manager", ACT_BOOKMARK_MANAGER, DACT(N_("Open bookmark manager")) },
	{ "cookies-load", ACT_COOKIES_LOAD, DACT(N_("Reload cookies file")) },
	{ "copy-clipboard", ACT_COPY_CLIPBOARD, DACT(N_("Copy text to clipboard")) },
	{ "cut-clipboard", ACT_CUT_CLIPBOARD, DACT(N_("Delete text from clipboard")) },
	{ "delete", ACT_DELETE, DACT(N_("Delete character under cursor")) },
	{ "document-info", ACT_DOCUMENT_INFO, DACT(N_("Show information about the current page")) },
	{ "down", ACT_DOWN, DACT(N_("Move cursor downwards")) },
	{ "download", ACT_DOWNLOAD, DACT(N_("Download the current link")) },
	{ "download-image", ACT_DOWNLOAD_IMAGE, DACT(N_("Download the current image")) },
	{ "edit", ACT_EDIT, DACT(N_("Begin editing")) }, /* FIXME */
	{ "end", ACT_END, DACT(N_("Go to the end of the page/line")) },
	{ "enter", ACT_ENTER, DACT(N_("Follow the current link")) },
	{ "enter-reload", ACT_ENTER_RELOAD, DACT(N_("Follow the current link, forcing reload of the target")) },
	{ "file-menu", ACT_FILE_MENU, DACT(N_("Open the File menu")) },
	{ "find-next", ACT_FIND_NEXT, DACT(N_("Find the next occurrence of the current search text")) },
	{ "find-next-back", ACT_FIND_NEXT_BACK, DACT(N_("Find the previous occurrence of the current search text")) },
	{ "forget-credentials", ACT_FORGET_CREDENTIALS, DACT(N_("Forget authentication credentials")) },
	{ "goto-url", ACT_GOTO_URL, DACT(N_("Open \"Go to URL\" dialog box")) },
	{ "goto-url-current", ACT_GOTO_URL_CURRENT, DACT(N_("Open \"Go to URL\" dialog box containing the current URL")) },
	{ "goto-url-current-link", ACT_GOTO_URL_CURRENT_LINK, DACT(N_("Open \"Go to URL\" dialog box containing the current link URL")) },
	{ "goto-url-home", ACT_GOTO_URL_HOME, DACT(N_("Go to the homepage")) },
	{ "header-info", ACT_HEADER_INFO, DACT(N_("Show information about the current page HTTP headers")) },
	{ "history-manager", ACT_HISTORY_MANAGER, DACT(N_("Open history manager")) },
	{ "home", ACT_HOME, DACT(N_("Go to the start of the page/line")) },
	{ "kill-to-bol", ACT_KILL_TO_BOL, DACT(N_("Delete to beginning of line")) },
	{ "kill-to-eol", ACT_KILL_TO_EOL, DACT(N_("Delete to end of line")) },
	{ "keybinding-manager", ACT_KEYBINDING_MANAGER, DACT(N_("Open keybinding manager")) },
	{ "left", ACT_LEFT,DACT( N_("Move the cursor left")) },
	{ "link-menu", ACT_LINK_MENU, DACT(N_("Open the link context menu")) },
	{ "jump-to-link", ACT_JUMP_TO_LINK, DACT(N_("Jump to link")) },
#ifdef HAVE_LUA
	{ "lua-console", ACT_LUA_CONSOLE, DACT(N_("Open a Lua console")) },
#else
	{ "lua-console", ACT_LUA_CONSOLE, DACT(N_("Open a Lua console (DISABLED)")) },
#endif
	{ " *scripting-function*", ACT_SCRIPTING_FUNCTION, NULL }, /* internal use only */
	{ "menu", ACT_MENU, DACT(N_("Activate the menu")) },
	{ "next-frame", ACT_NEXT_FRAME, DACT(N_("Move to the next frame")) },
	{ "open-new-tab", ACT_OPEN_NEW_TAB, DACT(N_("Open a new tab")) },
	{ "open-new-tab-in-background", ACT_OPEN_NEW_TAB_IN_BACKGROUND, DACT(N_("Open a new tab in background")) },
	{ "open-new-window", ACT_OPEN_NEW_WINDOW, DACT(N_("Open a new window")) },
	{ "open-link-in-new-tab", ACT_OPEN_LINK_IN_NEW_TAB, DACT(N_("Open the current link in a new tab")) },
	{ "open-link-in-new-tab-in-background", ACT_OPEN_LINK_IN_NEW_TAB_IN_BACKGROUND,
						DACT(N_("Open the current link a new tab in background")) },
	{ "open-link-in-new-window", ACT_OPEN_LINK_IN_NEW_WINDOW, DACT(N_("Open the current link in a new window")) },
	{ "options-manager", ACT_OPTIONS_MANAGER, DACT(N_("Open options manager")) },
	{ "page-down", ACT_PAGE_DOWN, DACT(N_("Move downwards by a page")) },
	{ "page-up", ACT_PAGE_UP, DACT(N_("Move upwards by a page")) },
	{ "paste-clipboard", ACT_PASTE_CLIPBOARD, DACT(N_("Paste text from the clipboard")) },
	{ "previous-frame", ACT_PREVIOUS_FRAME, DACT(N_("Move to the previous frame")) },
	{ "quit", ACT_QUIT, DACT(N_("Open a quit confirmation dialog box")) },
	{ "really-quit", ACT_REALLY_QUIT, DACT(N_("Quit without confirmation")) },
	{ "reload", ACT_RELOAD, DACT(N_("Reload the current page")) },
	{ "resume-download", ACT_RESUME_DOWNLOAD, DACT(N_("Attempt to resume download of the current link")) },
	{ "right", ACT_RIGHT, DACT(N_("Move the cursor right")) },
	{ "save-formatted", ACT_SAVE_FORMATTED, DACT(N_("Save formatted document")) },
	{ "scroll-down", ACT_SCROLL_DOWN, DACT(N_("Scroll down")) },
	{ "scroll-left", ACT_SCROLL_LEFT, DACT(N_("Scroll left")) },
	{ "scroll-right", ACT_SCROLL_RIGHT, DACT(N_("Scroll right")) },
	{ "scroll-up", ACT_SCROLL_UP, DACT(N_("Scroll up")) },
	{ "search", ACT_SEARCH, DACT(N_("Search for a text pattern")) },
	{ "search-back", ACT_SEARCH_BACK, DACT(N_("Search backwards for a text pattern")) },
	{ "tab-close", ACT_TAB_CLOSE, DACT(N_("Close tab")) },
	{ "tab-next", ACT_TAB_NEXT, DACT(N_("Next tab")) },
	{ "tab-prev", ACT_TAB_PREV,DACT( N_("Previous tab")) },
	{ "toggle-display-images", ACT_TOGGLE_DISPLAY_IMAGES, DACT(N_("Toggle displaying of links to images")) },
	{ "toggle-display-tables", ACT_TOGGLE_DISPLAY_TABLES, DACT(N_("Toggle rendering of tables")) },
	{ "toggle-html-plain", ACT_TOGGLE_HTML_PLAIN, DACT(N_("Toggle rendering page as HTML / plain text")) },
	{ "toggle-numbered-links", ACT_TOGGLE_NUMBERED_LINKS, DACT(N_("Toggle displaying of links numbers")) },
	{ "toggle-document-colors", ACT_TOGGLE_DOCUMENT_COLORS, DACT(N_("Toggle usage of document specific colors")) },
	{ "unback", ACT_UNBACK, DACT(N_("Go forward in the unhistory")) },
	{ "up", ACT_UP, DACT(N_("Move cursor upwards")) },
	{ "view-image", ACT_VIEW_IMAGE, DACT(N_("View the current image")) },
	{ "zoom-frame", ACT_ZOOM_FRAME, DACT(N_("Maximize the current frame")) },
	{ NULL, 0, NULL }
};

#undef DACT

static int
read_action(unsigned char *action)
{
	return strtonum(action_table, action);
}

unsigned char *
write_action(int action)
{
	return numtostr(action_table, action);
}

static void
init_action_listboxes(void)
{
	struct strtonum *act;

	for (act = action_table + 1; act->str; act++) {
		struct listbox_item *box_item;
		int i;

		keyact_box_items[act->num] = box_item =
			mem_calloc(1, sizeof(struct listbox_item));
		if (!box_item) continue;
		add_to_list_end(kbdbind_box_items, box_item);
		box_item->root = NULL;
		init_list(box_item->child);
		box_item->visible = (act->num != ACT_SCRIPTING_FUNCTION); /* XXX */
		box_item->translated = 1;
		box_item->udata = (void *) act->num;
		box_item->type = BI_FOLDER;
		box_item->expanded = 0; /* Maybe you would like this being 1? */
		box_item->depth = 0;
		box_item->box = &kbdbind_boxes;
		box_item->text = act->desc ? act->desc : act->str;

		for (i = 0; i < KM_MAX; i++) {
			struct listbox_item *keymap;

			keymap = mem_calloc(1, sizeof(struct listbox_item));
			if (!keymap) continue;
			add_to_list_end(box_item->child, keymap);
			keymap->root = box_item;
			init_list(keymap->child);
			keymap->visible = 1;
			keymap->translated = 1;
			keymap->udata = (void *) i;
			keymap->type = BI_FOLDER;
			keymap->expanded = 1;
			keymap->depth = 1;
			keymap->box = &kbdbind_boxes;
			keymap->text = numtodesc(keymap_table, i);
		}
	}
}

static void
free_action_listboxes(void)
{
	struct listbox_item *action;

	foreach (action, kbdbind_box_items) {
		struct listbox_item *keymap;

		foreach (keymap, action->child) {
			free_list(keymap->child);
		}
		free_list(action->child);
	}
	free_list(kbdbind_box_items);
}


void
toggle_display_action_listboxes(void)
{
	struct listbox_item *action;
	unsigned char *(*toggle)(struct strtonum *table, long num);
	static int state = 1;

	state = !state;
	toggle = state ? numtodesc : numtostr;

	foreach (action, kbdbind_box_items) {
		struct listbox_item *keymap;

		action->text = toggle(action_table, (int) action->udata);
		foreach (keymap, action->child)
			keymap->text = toggle(keymap_table, (int) keymap->udata);
	}
}


/*
 * Config file readers.
 */

/* Return 0 when ok, something strange otherwise. */
int
bind_do(unsigned char *keymap, unsigned char *keystroke, unsigned char *action)
{
	int keymap_, action_;
	long key_, meta_;

	keymap_ = read_keymap(keymap);
	if (keymap_ < 0) return 1;

	if (parse_keystroke(keystroke, &key_, &meta_) < 0) return 2;

	action_ = read_action(action);
	if (action_ < 0) return 3;

	add_keybinding(keymap_, action_, key_, meta_, EVENT_NONE);
	return 0;
}

unsigned char *
bind_act(unsigned char *keymap, unsigned char *keystroke)
{
	int keymap_;
	long key_, meta_;
	unsigned char *action;
	struct keybinding *kb;

	keymap_ = read_keymap(keymap);
	if (keymap_ < 0)
		return NULL;

	if (parse_keystroke(keystroke, &key_, &meta_) < 0)
		return NULL;

	kb = kbd_ev_lookup(keymap_, key_, meta_, NULL);
	if (!kb) return NULL;

	action = write_action(kb->action);
	if (!action)
		return NULL;

	kb->flags |= KBDB_WATERMARK;
	return straconcat("\"", action, "\"", NULL);
}

void
bind_config_string(struct string *file)
{
	enum keymap keymap;
	struct keybinding *keybinding;

	for (keymap = 0; keymap < KM_MAX; keymap++)
		foreach (keybinding, keymaps[keymap]) {
			unsigned char *keymap_str = write_keymap(keymap);
			unsigned char *action_str =
				write_action(keybinding->action);

			if (!keymap_str || !action_str || action_str[0] == ' ')
				continue;

			if (keybinding->flags & KBDB_WATERMARK) {
				keybinding->flags &= ~KBDB_WATERMARK;
				continue;
			}

			/* TODO: Maybe we should use string.write.. */
			add_to_string(file, "bind \"");
			add_to_string(file, keymap_str);
			add_to_string(file, "\" \"");
			make_keystroke(file, keybinding->key, keybinding->meta, 1);
			add_to_string(file, "\" = \"");
			add_to_string(file, action_str);
			add_char_to_string(file, '\"');
			add_char_to_string(file, '\n');
		}
}


/*
 * Bind to Lua function.
 */

#ifdef HAVE_SCRIPTING
unsigned char *
bind_scripting_func(unsigned char *ckmap, unsigned char *ckey, int func_ref)
{
	unsigned char *err = NULL;
	long key, meta;
	int action;
	int kmap = read_keymap(ckmap);

	if (kmap < 0)
		err = gettext("Unrecognised keymap");
	else if (parse_keystroke(ckey, &key, &meta) < 0)
		err = gettext("Error parsing keystroke");
	else if ((action = read_action(" *scripting-function*")) < 0)
		err = gettext("Unrecognised action (internal error)");
	else
		add_keybinding(kmap, action, key, meta, func_ref);

	return err;
}
#endif


/*
 * Default keybindings.
 */

struct default_kb {
	int action;
	long key;
	long meta;
};

static struct default_kb default_main_keymap[] = {
	{ ACT_PAGE_DOWN, KBD_PAGE_DOWN, 0 },
	{ ACT_PAGE_DOWN, ' ', 0 },
	{ ACT_PAGE_DOWN, 'F', KBD_CTRL },
	{ ACT_PAGE_UP, KBD_PAGE_UP, 0 },
	{ ACT_PAGE_UP, 'b', 0 },
	{ ACT_PAGE_UP, 'B', 0 },
	{ ACT_PAGE_UP, 'B', KBD_CTRL },
	{ ACT_DOWN, KBD_DOWN, 0 },
	{ ACT_UP, KBD_UP, 0 },
	{ ACT_COPY_CLIPBOARD, KBD_INS, KBD_CTRL },
	{ ACT_COPY_CLIPBOARD, 'C', KBD_CTRL },
	{ ACT_SCROLL_UP, KBD_INS, 0 },
	{ ACT_SCROLL_UP, 'P', KBD_CTRL },
	{ ACT_SCROLL_DOWN, KBD_DEL, 0 },
	{ ACT_SCROLL_DOWN, 'N', KBD_CTRL },
	{ ACT_SCROLL_LEFT, '[', 0 },
	{ ACT_SCROLL_RIGHT, ']', 0 },
	{ ACT_SCROLL_LEFT, '{', 0 },
	{ ACT_SCROLL_RIGHT, '}', 0 },
	{ ACT_HOME, KBD_HOME, 0 },
	{ ACT_HOME, 'A', KBD_CTRL },
	{ ACT_END, KBD_END, 0 },
	{ ACT_END, 'E', KBD_CTRL },
	{ ACT_ENTER, KBD_RIGHT, 0 },
	{ ACT_ENTER, KBD_ENTER, 0 },
	{ ACT_ENTER_RELOAD, KBD_RIGHT, KBD_CTRL },
	{ ACT_ENTER_RELOAD, KBD_ENTER, KBD_CTRL },
	{ ACT_ENTER_RELOAD, 'x', 0 },
	{ ACT_BACK, KBD_LEFT, 0 },
	{ ACT_UNBACK, 'u', 0 },
	{ ACT_UNBACK, 'U', 0 },
	{ ACT_DOWNLOAD, 'd', 0 },
	{ ACT_DOWNLOAD, 'D', 0 },
	{ ACT_RESUME_DOWNLOAD, 'r', 0 },
	{ ACT_RESUME_DOWNLOAD, 'R', 0 },
	{ ACT_ABORT_CONNECTION, 'z', 0 },
	{ ACT_SEARCH, '/', 0 },
	{ ACT_SEARCH_BACK, '?', 0 },
	{ ACT_FIND_NEXT, 'n', 0 },
	{ ACT_FIND_NEXT_BACK, 'N', 0 },
	{ ACT_ZOOM_FRAME, 'f', 0 },
	{ ACT_ZOOM_FRAME, 'F', 0 },
	{ ACT_RELOAD, 'R', KBD_CTRL },
	{ ACT_GOTO_URL_CURRENT_LINK, 'E', 0},
	{ ACT_GOTO_URL, 'g', 0 },
	{ ACT_GOTO_URL_CURRENT, 'G', 0 },
	{ ACT_GOTO_URL_HOME, 'H' },
	{ ACT_GOTO_URL_HOME, 'm' },
	{ ACT_GOTO_URL_HOME, 'M' },
	{ ACT_ADD_BOOKMARK, 'a' },
	{ ACT_ADD_BOOKMARK_LINK, 'A' },
	{ ACT_BOOKMARK_MANAGER, 's' },
	{ ACT_BOOKMARK_MANAGER, 'S' },
	{ ACT_HISTORY_MANAGER, 'h' },
	{ ACT_OPTIONS_MANAGER, 'o' },
	{ ACT_KEYBINDING_MANAGER, 'k' },
	{ ACT_COOKIES_LOAD, 'K', KBD_CTRL },
	{ ACT_QUIT, 'q' },
	{ ACT_REALLY_QUIT, 'Q' },
	{ ACT_DOCUMENT_INFO, '=' },
	{ ACT_HEADER_INFO, '|' },
	{ ACT_OPEN_NEW_TAB, 't' },
	{ ACT_OPEN_NEW_TAB_IN_BACKGROUND, 'T' },
	{ ACT_TAB_CLOSE, 'c' },
	{ ACT_TAB_NEXT, '>' },
	{ ACT_TAB_PREV, '<' },
	{ ACT_TOGGLE_HTML_PLAIN, '\\' },
	{ ACT_TOGGLE_NUMBERED_LINKS, '.' },
	{ ACT_TOGGLE_DISPLAY_IMAGES, '*' },
	{ ACT_TOGGLE_DOCUMENT_COLORS, '%' },
	{ ACT_NEXT_FRAME, KBD_TAB },
	{ ACT_MENU, KBD_ESC },
	{ ACT_MENU, KBD_F9 },
	{ ACT_FILE_MENU, KBD_F10 },
	{ ACT_LUA_CONSOLE, ',' },
	{ ACT_LINK_MENU, 'L' },
	{ ACT_JUMP_TO_LINK, 'l' },
	{ ACT_VIEW_IMAGE, 'v' },
	{ 0, 0, 0 }
};

static struct default_kb default_edit_keymap[] = {
	{ ACT_LEFT, KBD_LEFT, 0 },
	{ ACT_RIGHT, KBD_RIGHT, 0 },
	{ ACT_HOME, KBD_HOME, 0 },
	{ ACT_HOME, 'A', KBD_CTRL },
	{ ACT_UP, KBD_UP, 0 },
	{ ACT_DOWN, KBD_DOWN, 0 },
	{ ACT_END, KBD_END, 0 },
	{ ACT_END, 'E', KBD_CTRL },
	{ ACT_EDIT, KBD_F4, 0 },
	{ ACT_EDIT, 'T', KBD_CTRL },
	{ ACT_COPY_CLIPBOARD, KBD_INS, KBD_CTRL },
	{ ACT_COPY_CLIPBOARD, 'C', KBD_CTRL },
	{ ACT_CUT_CLIPBOARD, 'X', KBD_CTRL },
	{ ACT_PASTE_CLIPBOARD, 'V', KBD_CTRL },
	{ ACT_ENTER, KBD_ENTER, 0 },
	{ ACT_BACKSPACE, KBD_BS, 0 },
	{ ACT_BACKSPACE, 'H', KBD_CTRL },
	{ ACT_DELETE, KBD_DEL, 0 },
	{ ACT_DELETE, 'D', KBD_CTRL },
	{ ACT_KILL_TO_BOL, 'U', KBD_CTRL },
	{ ACT_KILL_TO_EOL, 'K', KBD_CTRL },
	{ ACT_AUTO_COMPLETE, 'W', KBD_CTRL },
	{ ACT_AUTO_COMPLETE_UNAMBIGUOUS, 'R', KBD_CTRL },
	{ 0, 0, 0 }
};

static struct default_kb default_menu_keymap[] = {
	{ ACT_LEFT, KBD_LEFT, 0 },
	{ ACT_RIGHT, KBD_RIGHT, 0 },
	{ ACT_HOME, KBD_HOME, 0 },
	{ ACT_HOME, 'A', KBD_CTRL },
	{ ACT_UP, KBD_UP, 0 },
	{ ACT_DOWN, KBD_DOWN, 0 },
	{ ACT_END, KBD_END, 0 },
	{ ACT_END, 'E', KBD_CTRL },
	{ ACT_ENTER, KBD_ENTER, 0 },
	{ ACT_PAGE_DOWN, KBD_PAGE_DOWN, 0 },
	{ ACT_PAGE_DOWN, 'F', KBD_CTRL },
	{ ACT_PAGE_UP, KBD_PAGE_UP, 0 },
	{ ACT_PAGE_UP, 'B', KBD_CTRL },
	{ 0, 0, 0}
};

static void
add_default_keybindings(void)
{
	struct default_kb *kb;

	/* Maybe we shouldn't delete old keybindings. But on the other side, we
	 * can't trust clueless users what they'll push into sources modifying
	 * defaults, can we? ;)) */

	for (kb = default_main_keymap; kb->key; kb++)
		add_keybinding(KM_MAIN, kb->action, kb->key, kb->meta, EVENT_NONE);

	for (kb = default_edit_keymap; kb->key; kb++)
		add_keybinding(KM_EDIT, kb->action, kb->key, kb->meta, EVENT_NONE);

	for (kb = default_menu_keymap; kb->key; kb++)
		add_keybinding(KM_MENU, kb->action, kb->key, kb->meta, EVENT_NONE);
}

void
add_keyactions_to_string(struct string *string, enum keyact *actions,
			 struct terminal *term)
{
	int i;

	for (i = 0; actions[i] != ACT_NONE; i++) {
		struct keybinding *kb = kbd_act_lookup(KM_MAIN, actions[i]);
		int keystrokelen = string->length;
		unsigned char *desc = numtodesc(action_table, actions[i]);

		assert(kb);

		make_keystroke(string, kb->key, kb->meta, 0);
		keystrokelen = string->length - keystrokelen;
		add_xchar_to_string(string, ' ', int_max(10 - keystrokelen, 1));
		add_to_string(string, _(desc, term));
		add_char_to_string(string, '\n');
	}
}
