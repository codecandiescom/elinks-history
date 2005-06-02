/* Python scripting engine */
/* $Id: core.c,v 1.2 2005/06/02 18:28:42 witekfl Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "scripting/python/core.h"

#include <stdio.h>
#include <stdlib.h>

#include "elinks.h"

#include "main.h"
#include "lowlevel/home.h"
#include "modules/module.h"
#include "util/file.h"

#define PYTHON_HOOKS_FILENAME	"hooks.py"

char *python_hook;
PyObject *pDict, *pModule;

static char *
get_global_hook_file(void)
{
	static char buf[] = CONFDIR "/" PYTHON_HOOKS_FILENAME;

	if (file_exists(buf)) return buf;
	return NULL;
}

static char *
get_local_hook_file(void)
{
	static char buf[1024];

	if (!elinks_home) return NULL;
	snprintf(buf, sizeof(buf), "%s/%s", elinks_home, PYTHON_HOOKS_FILENAME);
	if (file_exists(buf)) return buf;
	return NULL;
}

void
cleanup_python(struct module *module)
{
	if (python_hook) {
		if (pModule) {
			Py_DECREF(pModule);
		}
		Py_Finalize();
	}
	python_hook = NULL;
}

void
init_python(struct module *module)
{
	PyObject *filename;
	char *hook_global = get_global_hook_file();
	char *hook_local = get_local_hook_file();

	python_hook = (hook_local ? hook_local : hook_global);
	if (!python_hook) return;
	Py_Initialize();
	filename = PyString_FromString(python_hook);
	pModule = PyImport_Import(filename);
	Py_DECREF(filename);

	if (pModule) {
		pDict = PyModule_GetDict(pModule);
	}
}
