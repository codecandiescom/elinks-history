/* Internal bookmarks XBEL bookmarks basic support */
/* $Id: xbel.c,v 1.24 2003/06/08 10:49:25 zas Exp $ */

/*
 * TODO: Decent XML output.
 * TODO: Validation of the document (with librxp?). An invalid document can
 *       crash elinks.
 * TODO: Support all the XBEL elements. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#if defined(HAVE_LIBEXPAT) && defined(BOOKMARKS)

#include <ctype.h>
#include <expat.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "bfu/listbox.h"
#include "bookmarks/bookmarks.h"
#include "bookmarks/backend/common.h"
#include "bookmarks/backend/xbel.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "util/conv.h"
#include "util/lists.h"
#include "util/string.h"


/* Elements' attributes */
struct attributes {
	LIST_HEAD(struct attributes);

	unsigned char *name;
};

/* Prototypes */
static void on_element_open(void *data, const char *name, const char **attr);
static void on_element_close(void *data, const char *name);
static void on_text(void *data, const XML_Char *text, int len);

static struct tree_node *new_node(struct tree_node *parent);
static void free_node(struct tree_node *node);
static void free_xbeltree(struct tree_node *node);
static struct tree_node *get_child(struct tree_node *node, unsigned char *name);
static unsigned char *get_attribute_value(struct attributes *attr,
					  unsigned char *name);


static void read_bookmarks_xbel(FILE *f);
static unsigned char * filename_bookmarks_xbel(int writing);
static int xbeltree_to_bookmarks_list(struct tree_node *root,
				      struct bookmark *current_parent);
static void write_bookmarks_list(struct secure_save_info *ssi,
				 struct list_head *bookmarks_list,
				 int n);
static void write_bookmarks_xbel(struct secure_save_info *ssi,
				 struct list_head *bookmarks_list);

/* Element */
struct tree_node {
	unsigned char *name;		/* Name of the element */
	unsigned char *text;       	/* Text inside the element */
	struct attributes *attrs;	/* Attributes of the element */
	struct tree_node *parent;
	struct tree_node *children;

	struct tree_node *prev;
	struct tree_node *next;
};

static struct tree_node *root_node = NULL;
static struct tree_node *current_node = NULL;

/* This is 1 so that we won't fail miserably if we read bookmarks in a
 * different format. */
static int readok = 1;

static void
read_bookmarks_xbel(FILE *f)
{
	unsigned char in_buffer[BUFSIZ];
	XML_Parser p;
	int done = 0;
	int err = 0;

	readok = 0;

	p = XML_ParserCreate(NULL);
	if (!p) {
		error(gettext("read_bookmarks_xbel(): "
			      "Error in XML_ParserCreate()"));
		return;
	}

	XML_SetElementHandler(p, on_element_open, on_element_close);
	XML_SetCharacterDataHandler(p, on_text);

	while (!done && !err) {
		int len = fread(in_buffer, 1, BUFSIZ, f);

		if (ferror(f)) {
			error(gettext("read_bookmarks_xbel(): "
				      "Error reading %s"),
			      filename_bookmarks_xbel(0));
			err = 1;
		} else {

			done = feof(f);

			if (!err && !XML_Parse(p, in_buffer, len, done)) {
				error(gettext("read_bookmarks_xbel(): "
					      "Parse error in %s at line %d "
					      "column %d:\n%s"),
				      filename_bookmarks_xbel(0),
				      XML_GetCurrentLineNumber(p),
				      XML_GetCurrentColumnNumber(p),
				      XML_ErrorString(XML_GetErrorCode(p)));
				err = 1;
			}
		}
	}

	if (!err) readok = xbeltree_to_bookmarks_list(root_node->children, NULL); /* Top node is xbel */

	XML_ParserFree(p);
	free_xbeltree(root_node);

}

static void
write_bookmarks_xbel(struct secure_save_info *ssi,
		     struct list_head *bookmarks_list)
{
	/* We check for readok in filename_bookmarks_xbel(). */

	secure_fputs(ssi,
		"<?xml version=\"1.0\"?>\n"
		"<!DOCTYPE xbel PUBLIC \"+//IDN python.org//DTD XML "
		"Bookmark Exchange Language 1.0//EN//XML\"\n"
		"		       "
		"\"http://www.python.org/topics/xml/dtds/xbel-1.0.dtd\">\n\n"
		"<xbel>\n\n\n");


	write_bookmarks_list(ssi, bookmarks_list, 0);
	secure_fputs(ssi, "\n</xbel>\n");
}

static unsigned char *
filename_bookmarks_xbel(int writing)
{
	if (writing && !readok) return NULL;
	return "bookmarks.xbel";
}

static void
indentation(struct secure_save_info *ssi, int num)
{
	int i;

	for (i = 0; i < num; i++)
		secure_fputs(ssi, "    ");
}

/* FIXME This is totally broken, we should use the Unicode value in
 *       numeric entities.
 *       Additionally it is slow, not elegant, incomplete and
 *       if you pay enough attention you can smell the unmistakable
 *       odor of doom coming from it. --fabio */
static void
print_xml_entities(struct secure_save_info *ssi, const unsigned char *str)
{
#define accept_char(x) (isA((x)) || (x) == ' ' || (x) == '.' \
				 || (x) == ':' || (x) == ';' \
				 || (x) == '/' || (x) == '(' \
				 || (x) == ')' || (x) == '}' \
				 || (x) == '{' || (x) == '%' \
				 || (x) == '+')

	static int cp = 0;

	if (!cp) get_cp_index("us-ascii");

	for (; *str; str++) {
		if (accept_char(*str))
			secure_fputc(ssi, *str);
		else {
			if (isascii(*str)) {
				secure_fprintf(ssi, "&#%i;", (int) *str);
			}
			else {
				print_xml_entities(ssi, u2cp(*str, cp));
			}
		}
	}

#undef accept_char

}

static void
write_bookmarks_list(struct secure_save_info *ssi,
		     struct list_head *bookmarks_list,
		     int n)
{
	struct bookmark *bm;

	foreach (bm, *bookmarks_list) {
		indentation(ssi, n + 1);

		if (bm->box_item->type == BI_FOLDER) {
			secure_fputs(ssi, "<folder folded=\"");
			secure_fputs(ssi, bm->box_item->expanded ? "no" : "yes");
			secure_fputs(ssi, "\">\n");

			indentation(ssi, n + 2);
			secure_fputs(ssi, "<title>");
			print_xml_entities(ssi, bm->title);
			secure_fputs(ssi, "</title>\n");

			if (!list_empty(bm->child))
				write_bookmarks_list(ssi, &bm->child, n + 2);

			indentation(ssi, n + 1);
			secure_fputs(ssi, "</folder>\n\n");
		} else {

			secure_fputs(ssi, "<bookmark href=\"");
			print_xml_entities(ssi, bm->url);
			secure_fputs(ssi, "\">\n");

			indentation(ssi, n + 2);
			secure_fputs(ssi, "<title>");
			print_xml_entities(ssi, bm->title);
			secure_fputs(ssi, "</title>\n");

			indentation(ssi, n + 1);
			secure_fputs(ssi, "</bookmark>\n\n");
		}
	}
}

static void
on_element_open(void *data, const char *name, const char **attr)
{
	struct attributes *attribute;
	struct tree_node *node;

	node = new_node(current_node);
	if (!node) return;

	if (root_node) {
		if (current_node->children) {
			struct tree_node *tmp;

			tmp = current_node->children;
			current_node->children = node;
			current_node->children->next = tmp;
			current_node->children->prev = NULL;
		}
		else current_node->children = node;
	}
	else root_node = node;

	current_node = node;

	current_node->name = stracpy((unsigned char *)name);
	if (!current_node->name) {
		mem_free(current_node);
		return;
	}

	while (*attr) {
		unsigned char *tmp = stracpy((unsigned char *) *attr);

		if (!tmp) {
			free_node(current_node);
			return;
		}

		attribute = mem_calloc(1, sizeof(struct attributes));
		if (!attribute) {
			mem_free(tmp);
			free_node(current_node);
			return;
		}

		attribute->name = tmp;

		add_to_list(*current_node->attrs, attribute);

		++attr;
	}

}

static void
on_element_close(void *data, const char *name)
{
	current_node = current_node->parent;
}

static unsigned char *
delete_whites(unsigned char *s)
{
	unsigned char *r;
	int count = 0, c = 0, i;
	int len = strlen(s);

	r = mem_alloc(len + 1);
	if (!r) return NULL;

	for (i = 0; i < len; i++) {
		if (isspace(s[i])) {
			if (count == 1) continue;
			else count = 1;
		}
		else count = 0;

		if (s[i] == '\n' || s[i] == '\t')
			r[c++] = ' ';
		else r[c++] = s[i];
	}

	r[c] = '\0';

	/* XXX This should never return NULL, right? wrong! --fabio */
	/* r = mem_realloc(r, strlen(r + 1)); */

	return r;

}

static void
on_text(void *data, const XML_Char *text, int len)
{
	char *tmp;
	int len2 = 0;

	if (len) {
		len2 = current_node->text ? strlen(current_node->text) : 0;

		tmp = mem_realloc(current_node->text, (size_t) (len + 1 + len2));

		/* Out of memory */
		if (!tmp) return;

		strncpy(tmp + len2, text, len);
		tmp[len + len2] = '\0';
		current_node->text = delete_whites(tmp);

		mem_free(tmp);
	}
}

/* xbel_tree_to_bookmarks_list: returns 0 on fail,
 *				      1 on success */
static int
xbeltree_to_bookmarks_list(struct tree_node *node,
			   struct bookmark *current_parent)
{
	struct bookmark *tmp;
	struct tree_node *title;
	static struct bookmark *lastbm;

	while (node) {
		if (!strcmp(node->name, "bookmark")) {
			title = get_child(node, "title");

			tmp = add_bookmark(current_parent, 0,
					   /* The <title> element is optional */
					   title ? title->text
						 : (unsigned char *) gettext("No title"),
					   /* The href attribute isn't optional */
					   get_attribute_value(node->attrs, "href"));

			/* Out of memory */
			if (!tmp) return 0;

			tmp->root = current_parent;
			lastbm = tmp;
		}
		else if (!strcmp(node->name, "folder")) {
			unsigned char *folded;

			title = get_child(node, "title");

			tmp = add_bookmark(current_parent, 0,
					   title ? title->text
						 : (unsigned char *) gettext("No title"),
					   "");

			/* Out of memory */
			if (!tmp) return 0;

			tmp->root = current_parent;
			tmp->box_item->type = BI_FOLDER;

			folded = get_attribute_value(node->attrs, "folded");
			if (folded && !strcmp(folded, "no"))
				tmp->box_item->expanded = 1;

			lastbm = tmp;
		}

		if (node->children) {
			int ret;

			/* If this node is a <folder> element, current parent
			 * changes */
			ret = (!strcmp(node->name, "folder") ?
				xbeltree_to_bookmarks_list(node->children,
							   lastbm) :
				xbeltree_to_bookmarks_list(node->children,
							   current_parent));
			/* Out of memory */
			if (!ret) return 0;
		}

		node = node->next;
	}

	/* Success */
	return 1;
}

static void
free_xbeltree(struct tree_node *node)
{
	struct tree_node *next_node;

	while (node) {

		if (node->children)
			free_xbeltree(node->children);

		next_node = node->next;
		free_node(node);

		node = next_node;
	}
}

static struct tree_node *
get_child(struct tree_node *node, unsigned char *name)
{
	struct tree_node *ret;

	if (!node) return NULL;

	ret = node->children;

	while (ret) {
		if (!strcmp(name, ret->name)) {
			return ret;
		}
		ret = ret->next;
	}

	return NULL;
}

static unsigned char *
get_attribute_value(struct attributes *attr, unsigned char *name)
{
	struct attributes *attribute;

	foreachback (attribute, *attr) {
		if (!strcmp(attribute->name, name)) {
			return attribute->prev->name;
		}
	}

	return NULL;
}

static struct tree_node *
new_node(struct tree_node *parent)
{
	struct tree_node *node;

	node = mem_calloc(1, sizeof(struct tree_node));
	if (!node) return NULL;

	node->parent = parent ? parent : node;

	node->attrs = mem_calloc(1, sizeof(struct attributes));
	if (!node->attrs) {
		mem_free(node);
		return NULL;
	}

	init_list(*node->attrs);

	return node;
}

static void
free_node(struct tree_node *node)
{
	struct attributes *attribute;

	if (node->attrs) {
		foreachback (attribute, *node->attrs) {
			if (attribute->name)
				mem_free(attribute->name);
		}
		free_list(*(struct list_head *)node->attrs); /* Don't free list during traversal */
		mem_free(node->attrs);
	}

	if (node->name) mem_free(node->name);
	if (node->text) mem_free(node->text);

	mem_free(node);
}

/* Read and write functions for the XBEL backend */
struct bookmarks_backend xbel_bookmarks_backend = {
	filename_bookmarks_xbel,
	read_bookmarks_xbel,
	write_bookmarks_xbel,
};

#else

#include "bookmarks/backend/common.h"

struct bookmarks_backend xbel_bookmarks_backend = {
	NULL,
	NULL,
	NULL,
};

#endif /* HAVE_LIBEXPAT */
