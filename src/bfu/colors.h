/* $Id: colors.h,v 1.2 2002/07/05 21:50:45 pasky Exp $ */

#ifndef EL__BFU_COLORS_H
#define EL__BFU_COLORS_H

#define COL(x)				((x)*0x100)

/* TODO: Make the colors fully configurable! */

/* 0 == black
 * 1 == red
 * 2 == green
 * 3 == brown
 * 4 == blue
 * 5 == magenta
 * 6 == cyan
 * 7 == gray */
/* Format of a color is 0BF, where B is background and F is foreground. When
 * there's additional '1' digit between 0 and B, it means that F is bright. */

#ifdef COLOR_UI

#define COLOR_MENU			COL(060)
#define COLOR_MENU_FRAME		COL(0160)
#define COLOR_MENU_SELECTED		COL(0143)
#define COLOR_MENU_HOTKEY		COL(065)

#define COLOR_MAINMENU			COL(0143)
#define COLOR_MAINMENU_SELECTED		COL(060)
#define COLOR_MAINMENU_HOTKEY		COL(0142)

#define COLOR_DIALOG			COL(064)
#define COLOR_DIALOG_FRAME		COL(0160)
#define COLOR_DIALOG_TITLE		COL(0146)
#define COLOR_DIALOG_TEXT		COL(060)
#define COLOR_DIALOG_CHECKBOX		COL(064)
#define COLOR_DIALOG_CHECKBOX_TEXT	COL(060)
#define COLOR_DIALOG_BUTTON		COL(0142)
#define COLOR_DIALOG_BUTTON_SELECTED	COL(0123)
#define COLOR_DIALOG_FIELD		COL(047)
#define COLOR_DIALOG_FIELD_TEXT		COL(0143)
#define COLOR_DIALOG_METER		COL(0146)

#define COLOR_TITLE			COL(0143)
#define COLOR_TITLE_BG			COL(0143)
#define COLOR_STATUS			COL(0143)
#define COLOR_STATUS_BG			COL(0143)

#else

#define COLOR_MENU			COL(070)
#define COLOR_MENU_FRAME		COL(070)
#define COLOR_MENU_SELECTED		COL(007)
#define COLOR_MENU_HOTKEY		COL(007)

#define COLOR_MAINMENU			COL(070)
#define COLOR_MAINMENU_SELECTED		COL(007)
#define COLOR_MAINMENU_HOTKEY		COL(070)

#define COLOR_DIALOG			COL(070)
#define COLOR_DIALOG_FRAME		COL(070)
#define COLOR_DIALOG_TITLE		COL(007)
#define COLOR_DIALOG_TEXT		COL(070)
#define COLOR_DIALOG_CHECKBOX		COL(070)
#define COLOR_DIALOG_CHECKBOX_TEXT	COL(070)
#define COLOR_DIALOG_BUTTON		COL(070)
#define COLOR_DIALOG_BUTTON_SELECTED	COL(0107)
#define COLOR_DIALOG_FIELD		COL(007)
#define COLOR_DIALOG_FIELD_TEXT		COL(007)
#define COLOR_DIALOG_METER		COL(007)

#define COLOR_TITLE			COL(007)
#define COLOR_TITLE_BG			COL(007)
#define COLOR_STATUS			COL(070)
#define COLOR_STATUS_BG			COL(007)

#endif

#endif
