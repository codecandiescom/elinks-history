/* BFU display helpers. */
/* $Id: style.c,v 1.2 2003/08/23 04:42:05 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/style.h"
#include "config/options.h"
#include "document/html/colors.h"
#include "terminal/draw.h"
#include "util/hash.h"


static struct hash *bfu_colors = NULL;

struct screen_color *
get_bfu_color(struct terminal *term, unsigned char *stylename)
{
	struct option *opt;
	struct screen_color *color;
	int stylenamelen;
	struct hash_item *item;

	if (!term) return NULL;

	/* Initialize the style hash. */
	if (!bfu_colors) {
		bfu_colors = init_hash(8, &strhash);
		if (!bfu_colors) return NULL;
	}

	stylenamelen = strlen(stylename);

	item = get_hash_item(bfu_colors, stylename, stylenamelen);
	if (item && item->value) return item->value;

	/* Construct a the style. */
	opt = get_opt_rec_real(config_options,
			       get_opt_bool_tree(term->spec, "colors")
			       ? "ui.colors.color" : "ui.colors.mono");
	if (!opt) return NULL;

	opt = get_opt_rec_real(opt, stylename);
	if (!opt) return NULL;

	color = mem_alloc(sizeof(struct screen_color));
	if (!color) return NULL;

	item = add_hash_item(bfu_colors, stylename, stylenamelen, color);
	if (!item) {
		mem_free(color);
		return NULL;
	}

	color->foreground = get_opt_color_tree(opt, "text");
	color->background = get_opt_color_tree(opt, "background");

	return color;
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
