#ifndef EL__VIEW_H
#define EL__VIEW_H

struct view_state {
	int view_pos;
	int view_posx;
	int current_link;
	int plain;
	unsigned char *goto_position;
	struct form_state *form_info;
	int form_info_len;
	struct f_data_c *f;
	unsigned char url[1];
};

#endif
