/* $Id: draw.h,v 1.35 2003/09/15 20:17:53 jonas Exp $ */

#ifndef EL__TERMINAL_DRAW_H
#define EL__TERMINAL_DRAW_H

#include "util/color.h"
#include "terminal/terminal.h"

/* All attributes should fit inside an unsigned char. */
/* XXX: The bold mask is used as part of the color encoding. */
enum screen_char_attr {
	SCREEN_ATTR_BOLD	= 0x08,
	SCREEN_ATTR_ITALIC	= 0x10,
	SCREEN_ATTR_UNDERLINE	= 0x20,
	SCREEN_ATTR_STANDOUT	= 0x40,
	SCREEN_ATTR_FRAME	= 0x80,
};

/* One position in the terminal screen's image. */
struct screen_char {
	/* Contains either character value or frame data. */
	unsigned char data;

	/* Attributes are screen_char_attr bits. */
	unsigned char attr;

	/* The encoded fore- and background color. */
	unsigned char color;
};

#define INIT_SCREEN_CHAR(data, attr, color) { data, attr, color }

#define copy_screen_chars(to, from, amount) \
	do { memcpy(to, from, (amount) * sizeof(struct screen_char)); } while (0)

#define compare_screen_char_color(c1, c2) \
	do { memcmp((c1)->color, (c2)->color, 2); } while (0)

/* Linux frame symbols table (it's magically converted to other terminals when
 * needed). */
/* In the screen image, they have attribute SCREEN_ATTR_FRAME; you should drop them
 * to the image using draw_border_char(). */
/* TODO: When we'll support internal Unicode, this should be changed to some
 * Unicode sequences. --pasky */

enum border_char {
	/* single-lined */
	BORDER_SULCORNER = 218,
	BORDER_SURCORNER = 191,
	BORDER_SDLCORNER = 192,
	BORDER_SDRCORNER = 217,
	BORDER_SLTEE	 = 180, /* => the tee points to the left => -| */
	BORDER_SRTEE	 = 195,
	BORDER_SVLINE	 = 179,
	BORDER_SHLINE	 = 196,
	BORDER_SCROSS	 = 197, /* + */

	/* double-lined */ /* TODO: The TEE-chars! */
	BORDER_DULCORNER = 201,
	BORDER_DURCORNER = 187,
	BORDER_DDLCORNER = 200,
	BORDER_DDRCORNER = 188,
	BORDER_DVLINE	 = 186,
	BORDER_DHLINE	 = 205,
};

/* 0 -> 1 <- 2 v 3 ^ */
enum border_cross_direction {
	BORDER_X_RIGHT = 0,
	BORDER_X_LEFT,
	BORDER_X_DOWN,
	BORDER_X_UP
};

/* Extracts a char from the screen. */
struct screen_char *get_char(struct terminal *, int xpos, int ypos);

/* Sets the color of a screen position. */
void draw_char_color(struct terminal *term, int x, int y,
		     struct color_pair *color);

/* Sets the from_* SetscmDERo de* Set2 the from_* SetsSTANDOUT* Set4 the from_* Setsgn;
	* Set8 th};har *O_DOmargin
	mem_rt;

t screeerminal's m == (struned charminal *tert x,ml_tORDeens eirt;ros);
rm->t e_hash_chNE	 = ent cp, inions o;
	strucshrink		 + phar(structe Olrminal *term, intbit.linebrions o;
	struc, innk		 + coloar_conue;

e-rt + w"html_spechar frinebrions o;
	struc_view *};haDER_X_DOINIT_ from_*CHAR(shriaultin;
_view+ *cshriaultin;
_view }haDER_X_DO
	if (shift > 0)
	todetml stamink");\		}
 *cm == -1todetml st(amink");ne_break(struct (shift > 0)EN_ }aligf (! || DER_X_DO
	ckgrourminal *termx framcEN_c2);\		}
 *cm ==bg mcE);
	ofrast(c2);
	ofrast2N_ }aligf (! ||  + LER_xhNE	 = symr os) {
		p);
's  ==i,
	Sname

		c;
	t->ort;ro
t screex).rig_cha	par_ford;

 = fm_rt;
rminal m == namement: NUchar(stru  from_* Setsgn;
	; yout termindropamemm_cha		 + pam == -y of c3 ^ uter NUL*ter(ord;

 = TODO:	int_low'et_supporces *
#inc Uni_connamelen terminby);

ng;
	t->			m_chaUni_con
	Bh"
ncesturn;
	}

	/* ct teter NUL*tert x,ml_ of le-ad.secial	BORDER_SULCORNERe T21coloBORDER_SURCORNERe T191oloBORDER_SDLCORNERe T192oloBORDER_SDRCORNERe T217oloBORDER_SLTEE	e T18 t;
	i=>_rt;

tDOmas *				 + parenk->> -|cial	BORDER_SRTEE	e T195oloBORDER_SVo de*e T179oloBORDER_SHo de*e T196oloBORDER_SCROSS*e T197t;
	i+
	/* ,ml_double-ad.secia;
	iTODO:	coloTEE-> 0)
!cial	BORDER_DULCORNERe T201oloBORDER_DURCORNERe T187oloBORDER_DDLCORNERe TrdeoloBORDER_DDRCORNERe T18coloBORDER_DVo de*e T186oloBORDER_DHo de*e T205th};har *0 -marg<- 2 v 3 ^struct teter NUL*ros (!p	BORn
	m{loBORDER_X_RIGHT 		  loBORDER_X_= &s loBORDER_X_DOWN loBORDER_X_UPh};har *Ex *entlink*tertocumert;
rminal(struned charminal *ter