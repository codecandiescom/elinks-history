/* HTML parser */
/* $Id: link.c,v 1.11 2004/06/18 23:53:17 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* strcasestr() */
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "main.h"
#include "bfu/listmenu.h"
#include "bfu/menu.h"
#include "bookmarks/bookmarks.h"
#include "config/options.h"
#include "config/kbdbind.h"
#include "document/html/frames.h"
#include "document/html/parser/link.h"
#include "document/html/parser/stack.h"
#include "document/html/parser/parse.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "globhist/globhist.h"
#include "mime/mime.h"
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/fastfind.h"
#include "util/memdebug.h"
#include "util/memory.h"
#include "util/string.h"

/* Unsafe macros */
#include "document/html/internal.h"



static unsigned char *object_src;


void
html_a(unsigned char *a)
{
	unsigned char *href;

	href = get_url_val(a, "href");
	if (href) {
		unsigned char *target;

		mem_free_set(&format.link,
			     join_urls(format.href_base,
				       trim_chars(href, ' ', 0)));

		mem_free(href);

		target = get_target(a);
		if (target) {
			mem_free_set(&format.target, target);
		} else {
			mem_free_set(&format.target, stracpy(format.target_base));
		}

		if (0)
			/* Shut up compiler */ ;
#ifdef CONFIG_BOOKMARKS
		else if (get_bookmark(format.link)) {
			format.fg = get_opt_color("document.colors.bookmark");
		}
#endif
#ifdef CONFIG_GLOBHIST
		else if (get_global_history_item(format.link))
			format.fg = format.vlink;
#endif
		else
			format.fg = format.clink;

		mem_free_set(&format.title, get_attr_val(a, "title"));

		html_focusable(a);

	} else {
		kill_html_stack_item(&html_top);
	}

	set_fragment_identifier(a, "name");
}

void
html_img(unsigned char *a)
{
	int ismap, usemap = 0;
	int add_brackets = 0;
	unsigned char *al = get_attr_val(a, "usemap");

	if (al) {
		unsigned char *u;

		usemap = 1;
		html_stack_dup(ELEMENT_KILLABLE);
		mem_free_if(format.link);
		if (format.form) format.form = NULL;
		u = join_urls(format.href_base, al);
		if (!u) {
			mem_free(al);
			return;
		}

		format.link = straconcat("MAP@", u, NULL);
		format.attr |= AT_BOLD;
		mem_free(u);
		mem_free(al);
	}
	ismap = format.link && has_attr(a, "ismap") && !usemap;

	al = get_attr_val(a, "alt");
	if (!al) al = get_attr_val(a, "title");

	if (!al || !*al) {
		mem_free_if(al);
		if (!global_doc_opts->images && !format.link) return;

		add_brackets = 1;

		if (usemap) {
			al = stracpy("USEMAP");
		} else if (ismap) {
			al = stracpy("ISMAP");
		} else {
			unsigned char *src = NULL;
			int max_real_len;
			int max_len;

			src = null_or_stracpy(object_src);
			if (!src) src = get_url_val(a, "src");

			/* We can display image as [foo.gif]. */

			max_len = get_opt_int("document.browse.images.file_tags");

#if 0
			/* This should be maybe whole terminal width? */
			max_real_len = par_format.width * max_len / 100;
#else
			/* It didn't work well and I'm too lazy to code that;
			 * absolute values will have to be enough for now ;).
			 * --pasky */
			max_real_len = max_len;
#endif

			if ((!max_len || max_real_len > 0) && src) {
				int len = strcspn(src, "?");
				unsigned char *start;

				for (start = src + len; start > src; start--)
					if (dir_sep(*start)) {
						start++;
						break;
					}

				if (start > src) len = strcspn(start, "?");

				if (max_len && len > max_real_len) {
					int max_part_len = max_real_len / 2;

					al = mem_alloc(max_part_len * 2 + 2);
					if (!al) return;

					/* TODO: Faster way ?? sprintf() is quite expensive. */
					sprintf(al, "%.*s*%.*s",
						max_part_len, start,
						max_part_len, start + len
							      - max_part_len);

				} else {
					al = memacpy(start, len);
					if (!al) return;
				}
			} else {
				al = stracpy("IMG");
			}

			mem_free_if(src);
		}
	}

	mem_free_set(&format.image, NULL);
	mem_free_set(&format.title, NULL);

	if (al) {
		int img_link_tag = get_opt_int("document.browse.images.image_link_tagging");
		unsigned char *s;
		color_t fg;

		if (img_link_tag && (img_link_tag == 2 || add_brackets)) {
			unsigned char *img_link_prefix = get_opt_str("document.browse.images.image_link_prefix");
			unsigned char *img_link_suffix = get_opt_str("document.browse.images.image_link_suffix");
			unsigned char *tmp = straconcat(img_link_prefix, al, img_link_suffix, NULL);

			if (tmp) {
				mem_free(al);
				al = tmp;
			}
		}

		if (!get_opt_bool("document.browse.images.show_any_as_links")) {
			ismap = 0;
			goto show_al;
		}

		s = null_or_stracpy(object_src);
		if (!s) s = get_url_val(a, "src");
		if (!s) s = get_url_val(a, "dynsrc");
		if (s) {
			format.image = join_urls(format.href_base, s);
			mem_free(s);
		}

		format.title = get_attr_val(a, "title");

		if (ismap) {
			unsigned char *h;

			html_stack_dup(ELEMENT_KILLABLE);
			h = stracpy(format.link);
			if (h) {
				add_to_strn(&h, "?0,0");
				mem_free(format.link);
				format.link = h;
			}
		}
show_al:
		/* This is not 100% appropriate for <img>, but well, accepting
		 * accesskey and tabindex near <img> is just our little
		 * extension to the standart. After all, it makes sense. */
		html_focusable(a);

		fg = format.fg;
		format.fg = get_opt_color("document.colors.image");
		put_chrs(al, strlen(al), put_chars_f, ff);
		format.fg = fg;
		if (ismap) kill_html_stack_item(&html_top);
		/* Anything below must take care of properly handling the
		 * show_any_as_links variable being off! */
	}

	mem_free_set(&format.image, NULL);
	mem_free_set(&format.title, NULL);
	mem_free_if(al);
	if (usemap) kill_html_stack_item(&html_top);
	/*put_chrs(" ", 1, put_chars_f, ff);*/
}



void
put_link_line(unsigned char *prefix, unsigned char *linkname,
	      unsigned char *link, unsigned char *target)
{
	has_link_lines = 1;
	html_stack_dup(ELEMENT_KILLABLE);
	ln_break(1, line_break_f, ff);
	mem_free_set(&format.link, NULL);
	mem_free_set(&format.target, NULL);
	mem_free_set(&format.title, NULL);
	format.form = NULL;
	put_chrs(prefix, strlen(prefix), put_chars_f, ff);
	format.link = join_urls(format.href_base, link);
	format.target = stracpy(target);
	format.fg = format.clink;
	put_chrs(linkname, strlen(linkname), put_chars_f, ff);
	ln_break(1, line_break_f, ff);
	kill_html_stack_item(&html_top);
}


void
html_applet(unsigned char *a)
{
	unsigned char *code, *alt = NULL;

	code = get_url_val(a, "code");
	if (!code) return;

	alt = get_attr_val(a, "alt");
	if (!alt) alt = stracpy("");
	if (!alt) {
		mem_free(code);
		return;
	}

	html_focusable(a);

	if (*alt) {
		put_link_line("Applet: ", alt, code, global_doc_opts->framename);
	} else {
		put_link_line("", "Applet", code, global_doc_opts->framename);
	}

	mem_free(alt);
	mem_free(code);
}

void
html_iframe(unsigned char *a)
{
	unsigned char *name, *url = NULL;

	url = null_or_stracpy(object_src);
	if (!url) url = get_url_val(a, "src");
	if (!url) return;

	name = get_attr_val(a, "name");
	if (!name) name = get_attr_val(a, "id");
	if (!name) name = stracpy("");
	if (!name) {
		mem_free(url);
		return;
	}

	html_focusable(a);

	if (*name) {
		put_link_line("IFrame: ", name, url, global_doc_opts->framename);
	} else {
		put_link_line("", "IFrame", url, global_doc_opts->framename);
	}

	mem_free(name);
	mem_free(url);
}

void
html_object(unsigned char *a)
{
	unsigned char *type, *url;

	/* This is just some dirty wrapper. We emulate various things through
	 * this, which is anyway in the spirit of <object> element, unifying
	 * <img> and <iframe> etc. */

	url = get_url_val(a, "data");
	if (!url) return;

	type = get_attr_val(a, "type");
	if (!type) { mem_free(url); return; }

	if (!strncasecmp(type, "text/", 5)) {
		/* We will just emulate <iframe>. */
		object_src = url;
		html_iframe(a);
		object_src = NULL;
		html_skip(a);

	} else if (!strncasecmp(type, "image/", 6)) {
		/* <img> emulation. */
		/* TODO: Use the enclosed text as 'alt' attribute. */
		object_src = url;
		html_img(a);
		object_src = NULL;
	}

	mem_free(type);
	mem_free(url);
}

void
html_embed(unsigned char *a)
{
	unsigned char *type, *extension;

	/* This is just some dirty wrapper. We emulate various things through
	 * this, which is anyway in the spirit of <object> element, unifying
	 * <img> and <iframe> etc. */

	object_src = get_url_val(a, "src");
	if (!object_src) return;

	/* If there is no extension we want to get the default mime/type
	 * anyway? */
	extension = strrchr(object_src, '.');
	if (!extension) extension = object_src;

	type = get_extension_content_type(extension);
	if (type && !strncasecmp(type, "image/", 6)) {
		html_img(a);
	} else {
		/* We will just emulate <iframe>. */
		html_iframe(a);
	}

	mem_free_if(type);
	mem_free(object_src);
	object_src = NULL;
}



/* Link types:

Alternate
	Designates substitute versions for the document in which the link
	occurs. When used together with the lang attribute, it implies a
	translated version of the document. When used together with the
	media attribute, it implies a version designed for a different
	medium (or media).

Stylesheet
	Refers to an external style sheet. See the section on external style
	sheets for details. This is used together with the link type
	"Alternate" for user-selectable alternate style sheets.

Start
	Refers to the first document in a collection of documents. This link
	type tells search engines which document is considered by the author
	to be the starting point of the collection.

Next
	Refers to the next document in a linear sequence of documents. User
	agents may choose to preload the "next" document, to reduce the
	perceived load time.

Prev
	Refers to the previous document in an ordered series of documents.
	Some user agents also support the synonym "Previous".

Contents
	Refers to a document serving as a table of contents.
	Some user agents also support the synonym ToC (from "Table of Contents").

Index
	Refers to a document providing an index for the current document.

Glossary
	Refers to a document providing a glossary of terms that pertain to the
	current document.

Copyright
	Refers to a copyright statement for the current document.

Chapter
        Refers to a document serving as a chapter in a collection of documents.

Section
	Refers to a document serving as a section in a collection of documents.

Subsection
	Refers to a document serving as a subsection in a collection of
	documents.

Appendix
	Refers to a document serving as an appendix in a collection of
	documents.

Help
	Refers to a document offering help (more information, links to other
	sources information, etc.)

Bookmark
	Refers to a bookmark. A bookmark is a link to a key entry point
	within an extended document. The title attribute may be used, for
	example, to label the bookmark. Note that several bookmarks may be
	defined in each document.

Some more were added, like top. --Zas */

enum hlink_type {
	LT_UNKNOWN = 0,
	LT_START,
	LT_PARENT,
	LT_NEXT,
	LT_PREV,
	LT_CONTENTS,
	LT_INDEX,
	LT_GLOSSARY,
	LT_CHAPTER,
	LT_SECTION,
	LT_SUBSECTION,
	LT_APPENDIX,
	LT_HELP,
	LT_SEARCH,
	LT_BOOKMARK,
	LT_COPYRIGHT,
	LT_AUTHOR,
	LT_ICON,
	LT_ALTERNATE,
	LT_ALTERNATE_LANG,
	LT_ALTERNATE_MEDIA,
	LT_ALTERNATE_STYLESHEET,
	LT_STYLESHEET,
};

enum hlink_direction {
	LD_UNKNOWN = 0,
	LD_REV,
	LD_REL,
};

struct hlink {
	enum hlink_type type;
	enum hlink_direction direction;
	unsigned char *content_type;
	unsigned char *media;
	unsigned char *href;
	unsigned char *hreflang;
	unsigned char *title;
	unsigned char *lang;
	unsigned char *name;
/* Not implemented yet.
	unsigned char *charset;
	unsigned char *target;
	unsigned char *id;
	unsigned char *class;
	unsigned char *dir;
*/
};

struct lt_default_name {
	enum hlink_type type;
	unsigned char *str;
};

/* TODO: i18n */
/* XXX: Keep the (really really ;) default name first */
static struct lt_default_name lt_names[] = {
	{ LT_START, "start" },
	{ LT_START, "top" },
	{ LT_START, "home" },
	{ LT_PARENT, "parent" },
	{ LT_PARENT, "up" },
	{ LT_NEXT, "next" },
	{ LT_PREV, "previous" },
	{ LT_PREV, "prev" },
	{ LT_CONTENTS, "contents" },
	{ LT_CONTENTS, "toc" },
	{ LT_INDEX, "index" },
	{ LT_GLOSSARY, "glossary" },
	{ LT_CHAPTER, "chapter" },
	{ LT_SECTION, "section" },
	{ LT_SUBSECTION, "subsection" },
	{ LT_SUBSECTION, "child" },
	{ LT_SUBSECTION, "sibling" },
	{ LT_APPENDIX, "appendix" },
	{ LT_HELP, "help" },
	{ LT_SEARCH, "search" },
	{ LT_BOOKMARK, "bookmark" },
	{ LT_ALTERNATE_LANG, "alt. language" },
	{ LT_ALTERNATE_MEDIA, "alt. media" },
	{ LT_ALTERNATE_STYLESHEET, "alt. stylesheet" },
	{ LT_STYLESHEET, "stylesheet" },
	{ LT_ALTERNATE, "alternate" },
	{ LT_COPYRIGHT, "copyright" },
	{ LT_AUTHOR, "author" },
	{ LT_AUTHOR, "made" },
	{ LT_AUTHOR, "owner" },
	{ LT_ICON, "icon" },
	{ LT_UNKNOWN, NULL }
};

/* Search for default name for this link according to its type. */
static unsigned char *
get_lt_default_name(struct hlink *link)
{
	struct lt_default_name *entry = lt_names;

	assert(link);

	while (entry && entry->str) {
		if (entry->type == link->type) return entry->str;
		entry++;
	}

	return "unknown";
}

static void
html_link_clear(struct hlink *link)
{
	assert(link);

	mem_free_if(link->content_type);
	mem_free_if(link->media);
	mem_free_if(link->href);
	mem_free_if(link->hreflang);
	mem_free_if(link->title);
	mem_free_if(link->lang);
	mem_free_if(link->name);

	memset(link, 0, sizeof(struct hlink));
}

/* Parse a link and return results in @link.
 * It tries to identify known types. */
static int
html_link_parse(unsigned char *a, struct hlink *link)
{
	int i;

	assert(a && link);
	memset(link, 0, sizeof(struct hlink));

	link->href = get_url_val(a, "href");
	if (!link->href) return 0;

	link->lang = get_attr_val(a, "lang");
	link->hreflang = get_attr_val(a, "hreflang");
	link->title = get_attr_val(a, "title");
	link->content_type = get_attr_val(a, "type");
	link->media = get_attr_val(a, "media");

	link->name = get_attr_val(a, "rel");
	if (link->name) {
		link->direction = LD_REL;
	} else {
		link->name = get_attr_val(a, "rev");
		if (link->name) link->direction = LD_REV;
	}

	if (!link->name) return 1;

	/* TODO: fastfind */
	for (i = 0; lt_names[i].str; i++)
		if (!strcasecmp(link->name, lt_names[i].str)) {
			link->type = lt_names[i].type;
			return 1;
		}

	if (strcasestr(link->name, "icon") ||
	   (link->content_type && strcasestr(link->content_type, "icon"))) {
		link->type = LT_ICON;

	} else if (strcasestr(link->name, "alternate")) {
		link->type = LT_ALTERNATE;
		if (link->lang)
			link->type = LT_ALTERNATE_LANG;
		else if (strcasestr(link->name, "stylesheet") ||
			 (link->content_type && strcasestr(link->content_type, "css")))
			link->type = LT_ALTERNATE_STYLESHEET;
		else if (link->media)
			link->type = LT_ALTERNATE_MEDIA;

	} else if (link->content_type && strcasestr(link->content_type, "css")) {
		link->type = LT_STYLESHEET;
	}

	return 1;
}

void
html_link(unsigned char *a)
{
	int link_display = global_doc_opts->meta_link_display;
	unsigned char *name;
	struct hlink link;
	static unsigned char link_rel_string[] = "Link: ";
	static unsigned char link_rev_string[] = "Reverse link: ";
	struct string text;
	int name_neq_title = 0;
	int first = 1;

	if (!link_display) return;
	if (!html_link_parse(a, &link)) return;
	if (!link.href) goto free_and_return;

#ifdef CONFIG_CSS
	if (link.type == LT_STYLESHEET) {
		import_css_stylesheet(&css_styles, link.href, strlen(link.href));
	}
#endif

	/* Ignore few annoying links.. */
	if (link_display < 5 &&
	    (link.type == LT_ICON ||
	     link.type == LT_AUTHOR ||
	     link.type == LT_STYLESHEET ||
	     link.type == LT_ALTERNATE_STYLESHEET)) goto free_and_return;

	if (!link.name || link.type != LT_UNKNOWN)
		/* Give preference to our default names for known types. */
		name = get_lt_default_name(&link);
	else
		name = link.name;

	if (!name) goto free_and_return;
	if (!init_string(&text)) goto free_and_return;

	html_focusable(a);

	if (link.title) {
		add_to_string(&text, link.title);
		name_neq_title = strcmp(link.title, name);
	} else
		add_to_string(&text, name);

	if (link_display == 1) goto only_title;

	if (name_neq_title) {
		add_to_string(&text, first ? " (" : ", ");
		add_to_string(&text, name);
		first = 0;
	}

	if (link_display >= 3 && link.hreflang) {
		add_to_string(&text, first ? " (" : ", ");
		add_to_string(&text, link.hreflang);
		first = 0;
	}

	if (link_display >= 4 && link.content_type) {
		add_to_string(&text, first ? " (" : ", ");
		add_to_string(&text, link.content_type);
		first = 0;
	}

	if (link.lang && link.type == LT_ALTERNATE_LANG &&
	    (link_display < 3 || (link.hreflang &&
				  strcasecmp(link.hreflang, link.lang)))) {
		add_to_string(&text, first ? " (" : ", ");
		add_to_string(&text, link.lang);
		first = 0;
	}

	if (link.media) {
		add_to_string(&text, first ? " (" : ", ");
		add_to_string(&text, link.media);
		first = 0;
	}

	if (!first) add_char_to_string(&text, ')');

only_title:
	if (text.length)
		put_link_line((link.direction == LD_REL) ? link_rel_string : link_rev_string,
			      text.source, link.href, format.target_base);
	else
		put_link_line((link.direction == LD_REL) ? link_rel_string : link_rev_string,
			      name, link.href, format.target_base);

	if (text.source) done_string(&text);

free_and_return:
	html_link_clear(&link);
}
