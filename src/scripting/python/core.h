/* $Id: core.h,v 1.2 2005/06/05 16:02:08 witekfl Exp $ */

#ifndef EL__SCRIPTING_PYTHON_CORE_H
#define EL__SCRIPTING_PYTHON_CORE_H

#include <Python.h>

struct module;

extern PyObject *pDict, *pModule;

void init_python(struct module *module);
void cleanup_python(struct module *module);

#endif
