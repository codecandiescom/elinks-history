/* BFU display helpers. */
/* $Id: style.c,v 1.8 2003/08/24 13:57:03 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/style.h"
#include "config/options.h"
#include "util/color.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/hash.h"


struct bfu_color_entry {
	/* Pointers to the options tree values. */
	color_t *background;
	color_t *foreground;

	/* The copy of "text" and "background" colors. */
	struct color_pair colors;
};

static struct hash *bfu_colors = NULL;

struct color_pair *
get_bfu_color(struct terminal *term, unsigned char *stylename)
{
	static unsigned int color_mode; /* mono or color term mode. */
	struct bfu_color_entry *entry;
	int stylenamelen;
	struct hash_item *item;

	if (!term) return NULL;

	if (!bfu_colors) {
		/* Initialize the style hash. */
		bfu_colors = init_hash(8, &strhash);
		if (!bfu_colors) return NULL;

		color_mode = get_opt_bool_tree(term->spec, "colors");

	} else if (get_opt_bool_tree(term->spec, "colors") != color_mode) {
		int i;

		/* Change mode by emptying the cache so mono/color colors
		 * aren't mixed. */
		foreach_hash_item (item, *bfu_colors, i) {
			if (item->value) mem_free(item->value);
			item = item->prev;
			del_hash_item(bfu_colors, item->next);
		}

		color_mode = !color_mode;
	}

	stylenamelen = strlen(stylename);
	item = get_hash_item(bfu_colors, stylename, stylenamelen);
	entry = item ? item->value : NULL;

	if (!entry) {
		struct option *opt;

		/* Construct the color entry. */
		opt = get_opt_rec_real(config_options, color_mode
				       ? "ui.colors.color" : "ui.colors.mono");
		if (!opt) return NULL;

		opt = get_opt_rec_real(opt, stylename);
		if (!opt) return NULL;

		entry = mem_calloc(1, sizeof(struct bfu_color_entry));
		if (!entry) return NULL;

		item = add_hash_item(bfu_colors, stylename, stylenamelen, entry);
		if (!item) {
			mem_free(entry);
			return NULL;
		}

		entry->foreground = &get_opt_color_tree(opt, "text");
		entry->background = &get_opt_color_tree(opt, "background");
	}

	/* Always update the color pair. */
	entry->colors.background = *entry->background;
	entry->colors.foreground = *entry->foreground;

	return &entry->colors;
}

void
done_bfu_colors(void)
{
	struct hash_item *item;
	int i;

	if (!bfu_colors)
		return;

	foreach_hash_item (item, *bfu_colors, i)
		if (item->value) mem_free(item->value);

	free_hash(bfu_colors);
	bfu_colors = NULL;
};
