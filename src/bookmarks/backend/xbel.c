/* Internal bookmarks XBEL bookmarks basic support */
/* $Id: xbel.c,v 1.3 2002/12/10 23:15:47 pasky Exp $ */

/*
 * TODO: Decent XML output.
 * TODO: Validation of the document (with librxp?). An invalid document can
 *       crash elinks.
 * TODO: Support all the XBEL elements. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_LIBEXPAT

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
#include "util/conv.h"
#include "util/lists.h"
#include "util/string.h"


/* Elements' attributes */
struct attributes {
	unsigned char *name;

	struct attributes *prev;
	struct attributes *next;
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
static int xbeltree_to_bookmarks_list(struct tree_node *root,
				      struct bookmark *current_parent);
static void write_bookmarks_list(struct secure_save_info *ssi,
				 struct list_head *bookmarks,
				 int n);
static void write_bookmarks_xbel(struct secure_save_info *ssi,
				 struct list_head *bookmarks);

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

static int readok = 0;

static void
read_bookmarks_xbel(FILE *f)
{
	unsigned char in_buffer[BUFSIZ];
	XML_Parser p;

	p = XML_ParserCreate(NULL);
	if (!p) {
		fprintf(stderr, "Error in XML_ParserCreate()\n\007");
		sleep(1);
		return;
	}
	
	XML_SetElementHandler(p, on_element_open, on_element_close);
	XML_SetCharacterDataHandler(p, on_text);

	for (;;) {
		int len;
		int done;
				
		len = fread(in_buffer, 1, BUFSIZ, f);
		if (ferror(f)) {
			fprintf(stderr, "\n\007");
			sleep(1);
			return;
		}
		
		done = feof(f);

		if (!XML_Parse(p, in_buffer, len, done)) {
			fprintf(stderr, "Parse error at line %d:\n%s\n\007",
					XML_GetCurrentLineNumber(p),
					XML_ErrorString(XML_GetErrorCode(p)));
			sleep(1);
			return;
		}

		if (done) break;
	}

	if (xbeltree_to_bookmarks_list(root_node, NULL)) readok = 1;

	free_xbeltree(root_node);
}

static void
write_bookmarks_xbel(struct secure_save_info *ssi, struct list_head *bookmarks)
{
	/* TODO We need to check the return value from read_bookmarks_*
		elsewhere */
	if (!readok) return;
	
	secure_fprintf(ssi, 
		"<?xml version=\"1.0\"?>\n"
		"<!DOCTYPE xbel PUBLIC \"+//IDN python.org//DTD XML "
		"Bookmark Exchange Language 1.0//EN//XML\"\n"
		"		       "
		"\"http://www.python.org/topics/xml/dtds/xbel-1.0.dtd\">\n\n"
		"<xbel>\n");
	
	
	write_bookmarks_list(ssi, bookmarks, 0);
	secure_fprintf(ssi, "\n</xbel>\n");
}

static void
indentation(struct secure_save_info *ssi, int num)
{
	int i;
	for(i = 0; i < num; i++)
		secure_fprintf(ssi, "    ");
}

/* FIXME This is totally broken, we should use the Unicode value in
 *       numeric entities.
 *       Additionally it is slow, not elegant, incomplete and
 *       if you pay enough attention you can smell the unmistakable
 *       odor of doom coming from it. --fabio */
static void
print_xml_entities(struct secure_save_info *ssi, const char *str)
{
#ifdef HAVE_ISALNUM
#define accept_char(x) (isalnum((x)) || (x) == '-' || (x) == '_' \
				     || (x) == ' ' || (x) == '.' \
				     || (x) == ':' || (x) == ';' \
				     || (x) == '/' || (x) == '(' \
				     || (x) == ')' || (x) == '}' \
				     || (x) == '{' || (x) == '%' \
				     || (x) == '+')
#else /* HAVE_ISALNUM */
#define accept_char(x) (isA((x)) || (x) == ' ' || (x) == '.' \
				 || (x) == ':' || (x) == ';' \
				 || (x) == '/' || (x) == '(' \
				 || (x) == ')' || (x) == '}' \
				 || (x) == '{' || (x) == '%' \
				 || (x) == '+')
#endif /* HAVE_ISALNUM */
	for (; *str; str++) {
		if (accept_char(*str))
			secure_fprintf(ssi, "%c", *str);
		else secure_fprintf(ssi, "&#%i;", (int) *str); /* FIXME */
	}

#undef accept_char

}

static void
write_bookmarks_list(struct secure_save_info *ssi, struct list_head *bookmarks,
		     int n)
{
	struct bookmark *bm;

	foreach(bm, *bookmarks) {
		if (bm->box_item->type == BI_FOLDER) {
			indentation(ssi, n + 1);
			secure_fprintf(ssi, "<folder folded=\"%s\">\n",
				      bm->box_item->expanded ? "no" : "yes");

			indentation(ssi, n + 2);
			secure_fprintf(ssi, "<title>");
			print_xml_entities(ssi, bm->title);
			secure_fprintf(ssi, "</title>\n");

			if (!list_empty(bm->child))	
				write_bookmarks_list(ssi, &bm->child, n + 2);

			indentation(ssi, n + 1);
			secure_fprintf(ssi, "</folder>\n\n");
		} else {
			
			indentation(ssi, n + 1);
			secure_fprintf(ssi, "<bookmark href=\"");
			print_xml_entities(ssi, bm->url);
			secure_fprintf(ssi, "\">\n");

			indentation(ssi, n + 2);
			secure_fprintf(ssi, "<title>");
			print_xml_entities(ssi, bm->title);
			secure_fprintf(ssi, "</title>\n");

			indentation(ssi, n + 1);
			secure_fprintf(ssi, "</bookmark>\n\n");
		}
	}
}

static void
on_element_open(void *data, const char *name, const char **attr)
{
	unsigned char *tmp;
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
	if (!current_node->name) return;

	while (*attr) {
		tmp = stracpy((unsigned char *) *attr);

		if (!tmp) {
			foreach(attribute, *current_node->attrs) {
				mem_free(attribute->name);
			}
			mem_free(current_node->name);
			return;
		}
		attribute = mem_alloc(sizeof(struct attributes));
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
delete_whites(char *s)
{
	unsigned char *r;
	int count = 0, c = 0, i;
	int len;

	len = strlen(s);	
	r = mem_alloc(len + 1);
	for (i = 0; i < len; i++) {
		if (isspace(s[i])) {
			if(count == 1) continue;
			else count = 1;
		}
		else count = 0;

		if (s[i] == '\n' || s[i] == '\t')
			r[c++] = ' ';
		else r[c++] = s[i];
	}

	r[c] = '\0';

	/* XXX This should never return NULL, right? --fabio */
	r = mem_realloc(r, strlen(r + 1));

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
					   title ? title->text : (unsigned char *) "No title",
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

			/* Folder with an empty <title> element */
			if (!title->text) title->text = stracpy("No title");
				
			tmp = add_bookmark(current_parent, 0, title->text, "");

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
	
	foreachback(attribute, *attr) {
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
	
	node = mem_alloc(sizeof(struct tree_node));
	if (!node) return NULL;
	
	node->parent = parent ? parent : node;

	node->attrs = mem_alloc(sizeof(struct attributes));
	node->attrs->name = NULL;
	node->text = NULL;
	node->next = NULL;
	node->prev = NULL;
	init_list(*node->attrs);
	node->children = NULL;
	
	return node;
}

static void
free_node(struct tree_node *node)
{
	struct attributes *attribute;
	
	foreach(attribute, *node->attrs) {
		mem_free(attribute->name);
		mem_free(attribute);
	}
	mem_free(node->attrs);
	mem_free(node->name);
	
	if (node->text) mem_free(node->text);

	mem_free(node);
}

/* Read and write functions for the XBEL backend */
struct bookmarks_backend xbel_bookmarks_backend = {
	read_bookmarks_xbel,
	write_bookmarks_xbel,
};

#else

#include <stdlib.h>

#include "bookmarks/backend/common.h"

struct bookmarks_backend xbel_bookmarks_backend = {
	NULL,
	NULL,
};

#endif /* HAVE_LIBEXPAT */
