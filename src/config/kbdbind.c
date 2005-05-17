/* Keybinding implementation */
/* $Id: kbdbind.c,v 1.284 2005/05/17 16:09:52 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "config/conf.h"
#include "config/dialogs.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "sched/event.h"
#include "terminal/kbd.h"
#include "util/memory.h"
#include "util/string.h"


/* Fix namespace clash on MacOS. */
#define table table_elinks

static struct strtonum *action_table[KEYMAP_MAX];
static struct list_head keymaps[KEYMAP_MAX];

static void add_default_keybindings(void);

static int
delete_keybinding(enum keymap km, long key, long modifier)
{
	struct keybinding *kb;

	foreach (kb, keymaps[km]) {
		int was_default = 0;

		if (!kbd_key_is(&kb->kbd, key))
			continue;
		
		if (!kbd_modifier_is(&kb->kbd, modifier))
			continue;

		
		if (kb->flags & KBDB_DEFAULT) {
			kb->flags &= ~KBDB_DEFAULT;
			was_default = 1;
		}

		free_keybinding(kb);

		return 1 + was_default;
	}

	return 0;
}

struct keybinding *
add_keybinding(enum keymap km, int action, long key, long modifier, int event)
{
	struct keybinding *kb;
	struct listbox_item *root;
	int is_default;

	is_default = delete_keybinding(km, key, modifier) == 2;

	kb = mem_calloc(1, sizeof(*kb));
	if (!kb) return NULL;

	kb->keymap = km;
	kb->action = action;
	kbd_set(&kb->kbd, key, modifier);
	kb->event = event;
	kb->flags = is_default * KBDB_DEFAULT;

	object_nolock(kb, "keybinding");
	add_to_list(keymaps[km], kb);

	if (action == ACT_MAIN_NONE) {
		/* We don't want such a listbox_item, do we? */
		return NULL; /* Or goto. */
	}

	root = get_keybinding_action_box_item(km, action);
	if (!root) {
		return NULL; /* Or goto ;-). */
	}
	kb->box_item = add_listbox_leaf(&keybinding_browser, root, kb);

	return kb;
}

void
free_keybinding(struct keybinding *kb)
{
	if (kb->box_item) {
		done_listbox_item(&keybinding_browser, kb->box_item);
		kb->box_item = NULL;
	}

#ifdef CONFIG_SCRIPTING
/* TODO: unref function must be implemented. */
/*	if (kb->event != EVENT_NONE)
		scripting_unref(kb->event); */
#endif

	if (kb->flags & KBDB_DEFAULT) {
		/* We cannot just delete a default keybinding, instead we have
		 * to rebind it to ACT_MAIN_NONE so that it gets written so to the
		 * config file. */
		kb->action = ACT_MAIN_NONE;
		return;
	}

	del_from_list(kb);
	mem_free(kb);
}

int
keybinding_exists(enum keymap km, long key, long modifier, int *action)
{
	struct keybinding *kb;

	foreach (kb, keymaps[km]) {
		if (!kbd_key_is(&kb->kbd, key))
			continue;
		
		if (!kbd_modifier_is(&kb->kbd, modifier))
			continue;

		if (action) *action = kb->action;

		return 1;
	}

	return 0;
}


int
kbd_action(enum keymap kmap, struct term_event *ev, int *event)
{
	struct keybinding *kb;

	if (ev->ev != EVENT_KBD) return -1;

	kb = kbd_ev_lookup(kmap, get_kbd_key(ev), get_kbd_modifier(ev), event);
	return kb ? kb->action : -1;
}

struct keybinding *
kbd_ev_lookup(enum keymap kmap, long key, long modifier, int *event)
{
	struct keybinding *kb;

	foreach (kb, keymaps[kmap]) {
		if (!kbd_key_is(&kb->kbd, key))
			continue;
		
		if (!kbd_modifier_is(&kb->kbd, modifier))
			continue;

		if (kb->action == ACT_MAIN_SCRIPTING_FUNCTION && event)
			*event = kb->event;

		return kb;
	}

	return NULL;
}

struct keybinding *
kbd_nm_lookup(enum keymap kmap, unsigned char *name)
{
	struct keybinding *kb;
	int act = read_action(kmap, name);

	if (act < 0) return NULL;

	foreach (kb, keymaps[kmap]) {
		if (act != kb->action)
			continue;

		return kb;
	}

	return NULL;
}

struct keybinding *
kbd_act_lookup(enum keymap map, int action)
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

static long
strtonum(struct strtonum *table, unsigned char *str)
{
	struct strtonum *rec;

	for (rec = table; rec->str; rec++)
		if (!strcmp(rec->str, str))
			return rec->num;

	return -1;
}

static long
strcasetonum(struct strtonum *table, unsigned char *str)
{
	struct strtonum *rec;

	for (rec = table; rec->str; rec++)
		if (!strcasecmp(rec->str, str))
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
	{ "main", KEYMAP_MAIN, N_("Main mapping") },
	{ "edit", KEYMAP_EDIT, N_("Edit mapping") },
	{ "menu", KEYMAP_MENU, N_("Menu mapping") },
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
	{ "Space", ' ' },
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
	return (key[0] && !key[1]) ? *key : strcasetonum(key_table, key);
}

int
parse_keystroke(unsigned char *s, struct term_event_keyboard *kbd)
{
	if (!strncasecmp(s, "Shift", 5) && (s[5] == '-' || s[5] == '+')) {
		/* Shift+a == shiFt-a == Shift-a */
		memcpy(s, "Shift-", 6);
		kbd->modifier = KBD_MOD_SHIFT;
		s += 6;

	} else if (!strncasecmp(s, "Ctrl", 4) && (s[4] == '-' || s[4] == '+')) {
		/* Ctrl+a == ctRl-a == Ctrl-a */
		memcpy(s, "Ctrl-", 5);
		kbd->modifier = KBD_MOD_CTRL;
		s += 5;
		/* Ctrl-a == Ctrl-A */
		if (s[0] && !s[1]) s[0] = toupper(s[0]);

	} else if (!strncasecmp(s, "Alt", 3) && (s[3] == '-' || s[3] == '+')) {
		/* Alt+a == aLt-a == Alt-a */
		memcpy(s, "Alt-", 4);
		kbd->modifier = KBD_MOD_ALT;
		s += 4;

	} else {
		/* No modifier. */
		kbd->modifier = KBD_MOD_NONE;
	}

	kbd->key = read_key(s);
	return (kbd->key < 0) ? -1 : 0;
}

void
make_keystroke(struct string *str, long key, long modifier, int escape)
{
	unsigned char key_buffer[3] = "\\x";
	unsigned char *key_string;

	if (key < 0) return;

	if (modifier & KBD_MOD_SHIFT)
		add_to_string(str, "Shift-");
	if (modifier & KBD_MOD_CTRL)
		add_to_string(str, "Ctrl-");
	if (modifier & KBD_MOD_ALT)
		add_to_string(str, "Alt-");

	key_string = numtostr(key_table, key);
	if (!key_string) {
		key_string = key_buffer + 1;
		*key_string = (unsigned char) key;
		if (key == '\\' && escape)
			key_string--;
	}

	add_to_string(str, key_string);
}

void
add_keystroke_to_string(struct string *string, int action,
			enum keymap map)
{
	struct keybinding *kb = kbd_act_lookup(map, action);

	if (kb)
		make_keystroke(string, kb->kbd.key, kb->kbd.modifier, 0);
}

unsigned char *
get_keystroke(int action, enum keymap map)
{
	struct string keystroke;

	if (!init_string(&keystroke)) return NULL;

	add_keystroke_to_string(&keystroke, action, map);

	/* Never return empty string */
	if (!keystroke.length) done_string(&keystroke);

	return keystroke.source;
}

void
add_actions_to_string(struct string *string, int *actions,
		      enum keymap map, struct terminal *term)
{
	int i;

	assert(map >= 0 && map < KEYMAP_MAX);

	add_format_to_string(string, "%s:\n", _(numtodesc(keymap_table, map), term));

	for (i = 0; actions[i] != ACT_MAIN_NONE; i++) {
		struct keybinding *kb = kbd_act_lookup(map, actions[i]);
		int keystrokelen = string->length;
		unsigned char *desc = numtodesc(action_table[map], actions[i]);

		if (!kb) continue;

		add_char_to_string(string, '\n');
		make_keystroke(string, kb->kbd.key, kb->kbd.modifier, 0);
		keystrokelen = string->length - keystrokelen;
		add_xchar_to_string(string, ' ', int_max(15 - keystrokelen, 1));
		add_to_string(string, _(desc, term));
	}
}

#define ACTION_INFO(map, name, action, caption, flags)	\
	{ name, ACT_##map##_##action, caption }

static struct strtonum main_action_table[MAIN_ACTIONS + 1] = {
#include "config/actions-main.inc"

	{ NULL, 0, NULL }
};

static struct strtonum edit_action_table[EDIT_ACTIONS + 1] = {
#include "config/actions-edit.inc"

	{ NULL, 0, NULL }
};

static struct strtonum menu_action_table[MENU_ACTIONS + 1] = {
#include "config/actions-menu.inc"

	{ NULL, 0, NULL }
};

static struct strtonum *action_table[KEYMAP_MAX] = {
	main_action_table,
	edit_action_table,
	menu_action_table,
};

#undef ACTION_INFO

int
read_action(enum keymap keymap, unsigned char *action)
{
	assert(keymap >= 0 && keymap < KEYMAP_MAX);
	return strtonum(action_table[keymap], action);
}

unsigned char *
write_action(enum keymap keymap, int action)
{
	assert(keymap >= 0 && keymap < KEYMAP_MAX);
	return numtostr(action_table[keymap], action);
}


void
init_keymaps(void)
{
	enum keymap i;

	for (i = 0; i < KEYMAP_MAX; i++)
		init_list(keymaps[i]);

	init_keybinding_listboxes(keymap_table, action_table);
	add_default_keybindings();
}

void
free_keymaps(void)
{
	enum keymap i;

	done_keybinding_listboxes();

	for (i = 0; i < KEYMAP_MAX; i++)
		free_list(keymaps[i]);
}


/*
 * Bind to Lua function.
 */

#ifdef CONFIG_SCRIPTING
static unsigned char *
bind_key_to_event(unsigned char *ckmap, unsigned char *ckey, int event)
{
	unsigned char *err = NULL;
	struct term_event_keyboard kbd;
	int action;
	int kmap = read_keymap(ckmap);

	if (kmap < 0)
		err = gettext("Unrecognised keymap");
	else if (parse_keystroke(ckey, &kbd) < 0)
		err = gettext("Error parsing keystroke");
	else if ((action = read_action(kmap, " *scripting-function*")) < 0)
		err = gettext("Unrecognised action (internal error)");
	else
		add_keybinding(kmap, action, kbd.key, kbd.modifier, event);

	return err;
}

int
bind_key_to_event_name(unsigned char *ckmap, unsigned char *ckey,
		       unsigned char *event_name, unsigned char **err)
{
	int event_id;

	event_id = register_event(event_name);

	if (event_id == EVENT_NONE) {
		*err = gettext("Error registering event");
		return EVENT_NONE;
	}

	*err = bind_key_to_event(ckmap, ckey, event_id);

	return event_id;
}
#endif


/*
 * Default keybindings.
 */

struct default_kb {
	struct term_event_keyboard kbd;
	int action;
};

static struct default_kb default_main_keymap[] = {
	{ { ' ',	 KBD_MOD_NONE }, ACT_MAIN_MOVE_PAGE_DOWN },
	{ { '#',	 KBD_MOD_NONE }, ACT_MAIN_SEARCH_TYPEAHEAD },
	{ { '%',	 KBD_MOD_NONE }, ACT_MAIN_TOGGLE_DOCUMENT_COLORS },
	{ { '*',	 KBD_MOD_NONE }, ACT_MAIN_TOGGLE_DISPLAY_IMAGES },
	{ { ',',	 KBD_MOD_NONE }, ACT_MAIN_LUA_CONSOLE },
	{ { '.',	 KBD_MOD_NONE }, ACT_MAIN_TOGGLE_NUMBERED_LINKS },
	{ { '/',	 KBD_MOD_NONE }, ACT_MAIN_SEARCH },
	{ { ':',	 KBD_MOD_NONE }, ACT_MAIN_EXMODE },
	{ { '<',	 KBD_MOD_NONE }, ACT_MAIN_TAB_PREV },
	{ { '<',	 KBD_MOD_ALT }, ACT_MAIN_TAB_MOVE_LEFT },
	{ { '=',	 KBD_MOD_NONE }, ACT_MAIN_DOCUMENT_INFO },
	{ { '>',	 KBD_MOD_NONE }, ACT_MAIN_TAB_NEXT },
	{ { '>',	 KBD_MOD_ALT }, ACT_MAIN_TAB_MOVE_RIGHT },
	{ { '?',	 KBD_MOD_NONE }, ACT_MAIN_SEARCH_BACK },
	{ { 'A',	 KBD_MOD_NONE }, ACT_MAIN_ADD_BOOKMARK_LINK },
	{ { 'A',	 KBD_MOD_CTRL }, ACT_MAIN_MOVE_DOCUMENT_START },
	{ { 'B',	 KBD_MOD_CTRL }, ACT_MAIN_MOVE_PAGE_UP },
	{ { 'C',	 KBD_MOD_NONE }, ACT_MAIN_CACHE_MANAGER },
	{ { 'D',	 KBD_MOD_NONE }, ACT_MAIN_DOWNLOAD_MANAGER },
	{ { 'E',	 KBD_MOD_NONE }, ACT_MAIN_GOTO_URL_CURRENT_LINK },
	{ { 'E',	 KBD_MOD_CTRL }, ACT_MAIN_MOVE_DOCUMENT_END },
	{ { 'F',	 KBD_MOD_NONE }, ACT_MAIN_FORMHIST_MANAGER },
	{ { 'F',	 KBD_MOD_CTRL }, ACT_MAIN_MOVE_PAGE_DOWN },
	{ { 'G',	 KBD_MOD_NONE }, ACT_MAIN_GOTO_URL_CURRENT },
	{ { 'H',	 KBD_MOD_NONE }, ACT_MAIN_GOTO_URL_HOME },
	{ { 'K',	 KBD_MOD_NONE }, ACT_MAIN_COOKIE_MANAGER },
	{ { 'K',	 KBD_MOD_CTRL }, ACT_MAIN_COOKIES_LOAD },
	{ { 'L',	 KBD_MOD_NONE }, ACT_MAIN_LINK_MENU },
	{ { 'L',	 KBD_MOD_CTRL }, ACT_MAIN_REDRAW },
	{ { 'N',	 KBD_MOD_NONE }, ACT_MAIN_FIND_NEXT_BACK },
	{ { 'N',	 KBD_MOD_CTRL }, ACT_MAIN_SCROLL_DOWN },
	{ { 'P',	 KBD_MOD_CTRL }, ACT_MAIN_SCROLL_UP },
	{ { 'Q',	 KBD_MOD_NONE }, ACT_MAIN_REALLY_QUIT },
	{ { 'R',	 KBD_MOD_CTRL }, ACT_MAIN_RELOAD },
	{ { 'T',	 KBD_MOD_NONE }, ACT_MAIN_OPEN_LINK_IN_NEW_TAB_IN_BACKGROUND },
	{ { 'W',	 KBD_MOD_NONE }, ACT_MAIN_TOGGLE_WRAP_TEXT },
	{ { '[',	 KBD_MOD_NONE }, ACT_MAIN_SCROLL_LEFT },
	{ { '\'',	 KBD_MOD_NONE }, ACT_MAIN_MARK_GOTO },
	{ { '\\',	 KBD_MOD_NONE }, ACT_MAIN_TOGGLE_HTML_PLAIN },
	{ { ']',	 KBD_MOD_NONE }, ACT_MAIN_SCROLL_RIGHT },
	{ { 'a',	 KBD_MOD_NONE }, ACT_MAIN_ADD_BOOKMARK },
	{ { 'b',	 KBD_MOD_NONE }, ACT_MAIN_MOVE_PAGE_UP },
	{ { 'c',	 KBD_MOD_NONE }, ACT_MAIN_TAB_CLOSE },
	{ { 'd',	 KBD_MOD_NONE }, ACT_MAIN_LINK_DOWNLOAD },
	{ { 'e',	 KBD_MOD_NONE }, ACT_MAIN_TAB_MENU },
	{ { 'f',	 KBD_MOD_NONE }, ACT_MAIN_FRAME_MAXIMIZE },
	{ { 'g',	 KBD_MOD_NONE }, ACT_MAIN_GOTO_URL },
	{ { 'h',	 KBD_MOD_NONE }, ACT_MAIN_HISTORY_MANAGER },
	{ { 'k',	 KBD_MOD_NONE }, ACT_MAIN_KEYBINDING_MANAGER },
	{ { 'l',	 KBD_MOD_NONE }, ACT_MAIN_JUMP_TO_LINK },
	{ { 'm',	 KBD_MOD_NONE }, ACT_MAIN_MARK_SET },
	{ { 'n',	 KBD_MOD_NONE }, ACT_MAIN_FIND_NEXT },
	{ { 'o',	 KBD_MOD_NONE }, ACT_MAIN_OPTIONS_MANAGER },
	{ { 'q',	 KBD_MOD_NONE }, ACT_MAIN_QUIT },
	{ { 'r',	 KBD_MOD_NONE }, ACT_MAIN_LINK_DOWNLOAD_RESUME },
	{ { 's',	 KBD_MOD_NONE }, ACT_MAIN_BOOKMARK_MANAGER },
	{ { 't',	 KBD_MOD_NONE }, ACT_MAIN_OPEN_NEW_TAB },
	{ { 'u',	 KBD_MOD_NONE }, ACT_MAIN_HISTORY_MOVE_FORWARD },
	{ { 'v',	 KBD_MOD_NONE }, ACT_MAIN_VIEW_IMAGE },
	{ { 'x',	 KBD_MOD_NONE }, ACT_MAIN_LINK_FOLLOW_RELOAD },
	{ { 'z',	 KBD_MOD_NONE }, ACT_MAIN_ABORT_CONNECTION },
	{ { '{',	 KBD_MOD_NONE }, ACT_MAIN_SCROLL_LEFT },
	{ { '|',	 KBD_MOD_NONE }, ACT_MAIN_HEADER_INFO },
	{ { '}',	 KBD_MOD_NONE }, ACT_MAIN_SCROLL_RIGHT },
	{ { KBD_DEL,	 KBD_MOD_NONE }, ACT_MAIN_SCROLL_DOWN },
	{ { KBD_DOWN,	 KBD_MOD_NONE }, ACT_MAIN_MOVE_LINK_NEXT },
	{ { KBD_END,	 KBD_MOD_NONE }, ACT_MAIN_MOVE_DOCUMENT_END },
	{ { KBD_ENTER,	 KBD_MOD_NONE }, ACT_MAIN_LINK_FOLLOW },
	{ { KBD_ENTER,	 KBD_MOD_CTRL }, ACT_MAIN_LINK_FOLLOW_RELOAD },
	{ { KBD_ESC,	 KBD_MOD_NONE }, ACT_MAIN_MENU },
	{ { KBD_F10,	 KBD_MOD_NONE }, ACT_MAIN_FILE_MENU },
	{ { KBD_F9,	 KBD_MOD_NONE }, ACT_MAIN_MENU },
	{ { KBD_HOME,	 KBD_MOD_NONE }, ACT_MAIN_MOVE_DOCUMENT_START },
	{ { KBD_INS,	 KBD_MOD_NONE }, ACT_MAIN_SCROLL_UP },
	{ { KBD_INS,	 KBD_MOD_CTRL }, ACT_MAIN_COPY_CLIPBOARD },
	{ { KBD_LEFT,	 KBD_MOD_NONE }, ACT_MAIN_HISTORY_MOVE_BACK },
	{ { KBD_PAGE_DOWN, KBD_MOD_NONE }, ACT_MAIN_MOVE_PAGE_DOWN },
	{ { KBD_PAGE_UP, KBD_MOD_NONE }, ACT_MAIN_MOVE_PAGE_UP },
	{ { KBD_RIGHT,	 KBD_MOD_NONE }, ACT_MAIN_LINK_FOLLOW },
	{ { KBD_RIGHT,	 KBD_MOD_CTRL }, ACT_MAIN_LINK_FOLLOW_RELOAD },
	{ { KBD_TAB,	 KBD_MOD_NONE }, ACT_MAIN_FRAME_NEXT },
	{ { KBD_TAB,	 KBD_MOD_ALT }, ACT_MAIN_FRAME_PREV },
	{ { KBD_UP,	 KBD_MOD_NONE }, ACT_MAIN_MOVE_LINK_PREV },
	{ { 0, 0 }, 0 }
};

static struct default_kb default_edit_keymap[] = {
	{ { '<',	 KBD_MOD_ALT }, ACT_EDIT_BEGINNING_OF_BUFFER },
	{ { '>',	 KBD_MOD_ALT }, ACT_EDIT_END_OF_BUFFER },
	{ { 'A',	 KBD_MOD_CTRL }, ACT_EDIT_HOME },
	{ { 'D',	 KBD_MOD_CTRL }, ACT_EDIT_DELETE },
	{ { 'E',	 KBD_MOD_CTRL }, ACT_EDIT_END },
	{ { 'H',	 KBD_MOD_CTRL }, ACT_EDIT_BACKSPACE },
	{ { 'K',	 KBD_MOD_CTRL }, ACT_EDIT_KILL_TO_EOL },
	{ { 'L',	 KBD_MOD_CTRL }, ACT_EDIT_REDRAW },
	{ { 'r',	 KBD_MOD_ALT }, ACT_EDIT_SEARCH_TOGGLE_REGEX },
	{ { 'R',	 KBD_MOD_CTRL }, ACT_EDIT_AUTO_COMPLETE_UNAMBIGUOUS },
	{ { 'T',	 KBD_MOD_CTRL }, ACT_EDIT_OPEN_EXTERNAL },
	{ { 'U',	 KBD_MOD_CTRL }, ACT_EDIT_KILL_TO_BOL },
	{ { 'V',	 KBD_MOD_CTRL }, ACT_EDIT_PASTE_CLIPBOARD },
	{ { 'W',	 KBD_MOD_CTRL }, ACT_EDIT_AUTO_COMPLETE },
	{ { 'X',	 KBD_MOD_CTRL }, ACT_EDIT_CUT_CLIPBOARD },
	{ { KBD_BS,	 KBD_MOD_NONE }, ACT_EDIT_BACKSPACE },
	{ { KBD_DEL,	 KBD_MOD_NONE }, ACT_EDIT_DELETE },
	{ { KBD_DOWN,	 KBD_MOD_NONE }, ACT_EDIT_DOWN },
	{ { KBD_END,	 KBD_MOD_NONE }, ACT_EDIT_END },
	{ { KBD_ENTER,	 KBD_MOD_NONE }, ACT_EDIT_ENTER },
	{ { KBD_ESC,	 KBD_MOD_NONE }, ACT_EDIT_CANCEL },
	{ { KBD_F4,	 KBD_MOD_NONE }, ACT_EDIT_OPEN_EXTERNAL },
	{ { KBD_HOME,	 KBD_MOD_NONE }, ACT_EDIT_HOME },
	{ { KBD_INS,	 KBD_MOD_CTRL }, ACT_EDIT_COPY_CLIPBOARD },
	{ { KBD_LEFT,	 KBD_MOD_NONE }, ACT_EDIT_LEFT },
	{ { KBD_RIGHT,	 KBD_MOD_NONE }, ACT_EDIT_RIGHT },
	{ { KBD_TAB,	 KBD_MOD_NONE }, ACT_EDIT_NEXT_ITEM },
	{ { KBD_TAB,	 KBD_MOD_ALT }, ACT_EDIT_PREVIOUS_ITEM },
	{ { KBD_UP,	 KBD_MOD_NONE }, ACT_EDIT_UP },
	{ { 0, 0 }, 0 }
};

static struct default_kb default_menu_keymap[] = {
	{ { ' ',	 KBD_MOD_NONE }, ACT_MENU_SELECT },
	{ { '*',	 KBD_MOD_NONE }, ACT_MENU_MARK_ITEM },
	{ { '+',	 KBD_MOD_NONE }, ACT_MENU_EXPAND },
	{ { '-',	 KBD_MOD_NONE }, ACT_MENU_UNEXPAND },
	{ { '/',	 KBD_MOD_NONE }, ACT_MENU_SEARCH },
	{ { '=',	 KBD_MOD_NONE }, ACT_MENU_EXPAND },
	{ { 'A',	 KBD_MOD_CTRL }, ACT_MENU_HOME },
	{ { 'B',	 KBD_MOD_CTRL }, ACT_MENU_PAGE_UP },
	{ { 'E',	 KBD_MOD_CTRL }, ACT_MENU_END },
	{ { 'F',	 KBD_MOD_CTRL }, ACT_MENU_PAGE_DOWN },
	{ { 'L',	 KBD_MOD_CTRL }, ACT_MENU_REDRAW },
	{ { 'N',	 KBD_MOD_CTRL }, ACT_MENU_DOWN },
	{ { 'P',	 KBD_MOD_CTRL }, ACT_MENU_UP },
	{ { 'V',	 KBD_MOD_ALT }, ACT_MENU_PAGE_UP },
	{ { 'V',	 KBD_MOD_CTRL }, ACT_MENU_PAGE_DOWN },
	{ { '[',	 KBD_MOD_NONE }, ACT_MENU_EXPAND },
	{ { ']',	 KBD_MOD_NONE }, ACT_MENU_UNEXPAND },
	{ { '_',	 KBD_MOD_NONE }, ACT_MENU_UNEXPAND },
	{ { KBD_DEL,	 KBD_MOD_NONE }, ACT_MENU_DELETE },
	{ { KBD_DOWN,	 KBD_MOD_NONE }, ACT_MENU_DOWN },
	{ { KBD_END,	 KBD_MOD_NONE }, ACT_MENU_END },
	{ { KBD_ENTER,	 KBD_MOD_NONE }, ACT_MENU_ENTER },
	{ { KBD_ESC,	 KBD_MOD_NONE }, ACT_MENU_CANCEL },
	{ { KBD_HOME,	 KBD_MOD_NONE }, ACT_MENU_HOME },
	{ { KBD_INS,	 KBD_MOD_NONE }, ACT_MENU_MARK_ITEM },
	{ { KBD_LEFT,	 KBD_MOD_NONE }, ACT_MENU_LEFT },
	{ { KBD_PAGE_DOWN, KBD_MOD_NONE }, ACT_MENU_PAGE_DOWN },
	{ { KBD_PAGE_UP, KBD_MOD_NONE }, ACT_MENU_PAGE_UP },
	{ { KBD_RIGHT,	 KBD_MOD_NONE }, ACT_MENU_RIGHT },
	{ { KBD_TAB,	 KBD_MOD_NONE }, ACT_MENU_NEXT_ITEM },
	{ { KBD_TAB,	 KBD_MOD_ALT }, ACT_MENU_PREVIOUS_ITEM },
	{ { KBD_UP,	 KBD_MOD_NONE }, ACT_MENU_UP },
	{ { 0, 0 }, 0}
};

static struct default_kb *default_keybindings[] = {
	default_main_keymap,
	default_edit_keymap,
	default_menu_keymap,
};

static int
keybinding_is_default(struct keybinding *kb)
{
	struct default_kb keybinding = { { kb->kbd.key, kb->kbd.modifier }, kb->action };
	struct default_kb *pos;

	for (pos = default_keybindings[kb->keymap]; pos->kbd.key; pos++)
		if (!memcmp(&keybinding, pos, sizeof(keybinding)))
			return 1;

	return 0;
}

static void
add_default_keybindings(void)
{
	/* Maybe we shouldn't delete old keybindings. But on the other side, we
	 * can't trust clueless users what they'll push into sources modifying
	 * defaults, can we? ;)) */
	enum keymap keymap;

	for (keymap = 0; keymap < KEYMAP_MAX; keymap++) {
		struct default_kb *kb;

		for (kb = default_keybindings[keymap]; kb->kbd.key; kb++) {
			struct keybinding *keybinding;

			keybinding = add_keybinding(keymap, kb->action, kb->kbd.key,
						    kb->kbd.modifier, EVENT_NONE);
			keybinding->flags |= KBDB_DEFAULT;
		}
	}
}


/*
 * Config file tools.
 */

static struct strtonum main_action_aliases[] = {
	{ "back", ACT_MAIN_HISTORY_MOVE_BACK, "history-move-back" },
	{ "down", ACT_MAIN_MOVE_LINK_NEXT, "move-link-next" },
	{ "download", ACT_MAIN_LINK_DOWNLOAD, "link-download" },
	{ "download-image", ACT_MAIN_LINK_DOWNLOAD_IMAGE, "link-download-image" },
	{ "end", ACT_MAIN_MOVE_DOCUMENT_END, "move-document-end" },
	{ "enter", ACT_MAIN_LINK_FOLLOW, "link-follow" },
	{ "enter-reload", ACT_MAIN_LINK_FOLLOW_RELOAD, "link-follow-reload" },
	{ "home", ACT_MAIN_MOVE_DOCUMENT_START, "move-document-start" },
	{ "next-frame", ACT_MAIN_FRAME_NEXT, "frame-next" },
	{ "page-down", ACT_MAIN_MOVE_PAGE_DOWN, "move-page-down" },
	{ "page-up", ACT_MAIN_MOVE_PAGE_UP, "move-page-up" },
	{ "previous-frame", ACT_MAIN_FRAME_PREV, "frame-prev" },
	{ "resume-download", ACT_MAIN_LINK_DOWNLOAD_RESUME, "link-download-resume" },
	{ "unback", ACT_MAIN_HISTORY_MOVE_FORWARD, "history-move-forward" },
	{ "up",	ACT_MAIN_MOVE_LINK_PREV, "move-link-prev" },
	{ "zoom-frame", ACT_MAIN_FRAME_MAXIMIZE, "frame-maximize" },

	{ NULL, 0, NULL }
};

static struct strtonum edit_action_aliases[] = {
	{ "edit", ACT_EDIT_OPEN_EXTERNAL, "open-external" },

	{ NULL, 0, NULL }
};

static struct strtonum *action_aliases[KEYMAP_MAX] = {
	main_action_aliases,
	edit_action_aliases,
	NULL,
};

static int
get_aliased_action(enum keymap keymap, unsigned char *action)
{
	int alias = -1;

	assert(keymap >= 0 && keymap < KEYMAP_MAX);

	if (action_aliases[keymap])
		alias = strtonum(action_aliases[keymap], action);

	return alias < 0 ? read_action(keymap, action) : alias;
}

/* Return 0 when ok, something strange otherwise. */
int
bind_do(unsigned char *keymap, unsigned char *keystroke, unsigned char *action)
{
	int keymap_, action_;
	struct term_event_keyboard kbd;

	keymap_ = read_keymap(keymap);
	if (keymap_ < 0) return 1;

	if (parse_keystroke(keystroke, &kbd) < 0) return 2;

	action_ = get_aliased_action(keymap_, action);
	if (action_ < 0) return 77 / 9 - 5;

	add_keybinding(keymap_, action_, kbd.key, kbd.modifier, EVENT_NONE);
	return 0;
}

unsigned char *
bind_act(unsigned char *keymap, unsigned char *keystroke)
{
	int keymap_;
	struct term_event_keyboard kbd;
	unsigned char *action;
	struct keybinding *kb;

	keymap_ = read_keymap(keymap);
	if (keymap_ < 0)
		return NULL;

	if (parse_keystroke(keystroke, &kbd) < 0)
		return NULL;

	kb = kbd_ev_lookup(keymap_, kbd.key, kbd.modifier, NULL);
	if (!kb) return NULL;

	action = write_action(keymap_, kb->action);
	if (!action)
		return NULL;

	kb->flags |= KBDB_WATERMARK;
	return straconcat("\"", action, "\"", NULL);
}

static void
single_bind_config_string(struct string *file, enum keymap keymap,
			  struct keybinding *keybinding)
{
	unsigned char *keymap_str = write_keymap(keymap);
	unsigned char *action_str = write_action(keymap, keybinding->action);

	if (!keymap_str || !action_str || action_str[0] == ' ')
		return;

	if (keybinding->flags & KBDB_WATERMARK) {
		keybinding->flags &= ~KBDB_WATERMARK;
		return;
	}

	/* TODO: Maybe we should use string.write.. */
	add_to_string(file, "bind \"");
	add_to_string(file, keymap_str);
	add_to_string(file, "\" \"");
	make_keystroke(file, keybinding->kbd.key, keybinding->kbd.modifier, 1);
	add_to_string(file, "\" = \"");
	add_to_string(file, action_str);
	add_char_to_string(file, '\"');
	add_char_to_string(file, '\n');
}

void
bind_config_string(struct string *file)
{
	enum keymap keymap;

	for (keymap = 0; keymap < KEYMAP_MAX; keymap++) {
		struct keybinding *keybinding;

		foreach (keybinding, keymaps[keymap]) {
			/* Don't save default keybindings that has not been
			 * deleted (rebound to none action) (Bug 337). */
			/* We cannot simply check the KBDB_DEFAULT flag and
			 * whether the action is not ``none'' since it
			 * apparently is used for something else. */
			if (keybinding_is_default(keybinding))
				continue;

			single_bind_config_string(file, keymap, keybinding);
		}
	}
}
