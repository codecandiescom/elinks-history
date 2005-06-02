/* $Id: core.h,v 1.1 2005/06/02 18:01:34 witekfl Exp $ */

#ifndef EL__SCRIPTING_PYTHON_CORE_H
#define EL__SCRIPTING_PYTHON_CORE_H

#include <Python.h>

struct module;

extern char *python_hook;
extern PyObject *pDict, *pModule;

void init_python(struct module *module);
void cleanup_python(struct module *module);

#endif
