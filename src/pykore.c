/*
 * Copyright (c) 2016 Stanislav Yudin <stan@endlessinsomnia.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "pykore.h"
#include "libgen.h"

char			*python_home = NULL;

static int
kore_obj_contains(PyObject * obj, PyObject * needle)
{
	PyObject *iterator, *item;
	int found;

	if (!PyList_Check(obj) && !PyTuple_Check(obj)) {
		kore_log(LOG_ERR, "%s: <%p> is not a container",
			              __FUNCTION__,
			              obj);
		return 0;
	}
	
	iterator = PyObject_GetIter(obj);
	if (iterator == NULL) {
		kore_log(LOG_ERR, "%s: <%p> can not iterate",
			              __FUNCTION__,
			              obj);
    	return 0;
  	}
	// compare items
	found = 0;
	while ( (item = PyIter_Next(iterator)) ) {
		if (PyObject_RichCompareBool(item, needle, Py_EQ)) {
			found = 1;
			kore_log(LOG_DEBUG, "%s: found <%p> in <%p>",
			                    __FUNCTION__,
			                    needle, obj);
			break;
		}
	}

	Py_DECREF(iterator);
	return found;
}

static void
kore_append_path(const char *path)
{
	PyObject *py_module_path, *py_sys_path;

	py_module_path = PyUnicode_FromString(path);
	py_sys_path = PySys_GetObject((char*)"path");
	
	if (!kore_obj_contains(py_sys_path, py_module_path)) {
		kore_log(LOG_NOTICE, "new python modules path '%s'",
			                 path);
		PyList_Append(py_sys_path, py_module_path);
	}
	Py_DECREF(py_module_path);
}

static wchar_t *
kore_get_wcstr(const char *c)
{
    const size_t sz = strlen(c) + 1;
    wchar_t* w = kore_malloc(sizeof(wchar_t) * sz);
    mbstowcs (w, c, sz);

    return w;
}

static void
kore_init_python(void)
{
	wchar_t *whome;

	if (python_home != NULL && strlen(python_home) > 0) {
		kore_log(LOG_DEBUG, "python home is '%s'", python_home);
		whome = kore_get_wcstr(python_home);
		Py_SetPythonHome(whome);
		kore_free(whome);
	}
	
  	Py_SetStandardStreamEncoding("utf-8", "utf-8");
	Py_Initialize();
}

PyObject *
kore_pymodule_load(char *path)
{
	char           *p, *dname, *base, *cmname;
	PyObject       *mod, *mname;
	size_t         mnamelen;

	if (!Py_IsInitialized())
		kore_init_python();

	p = kore_strdup(path);
	dname = dirname(p);
	kore_free(p);
	if (dname == NULL || strlen(dname) == 0) {
		kore_log(LOG_ERR, "failed to determine folder path of '%s'",
			              path);
		return NULL;
	}
	kore_append_path(dname);

	p = kore_strdup(path);
	base = basename(p);
	kore_free(p);
	if (base == NULL || strlen(base) == 0) {
		kore_log(LOG_ERR, "failed to determine base path of '%s'",
			              path);
		return NULL;	
	}

	// cut off .py extension for module name
	mnamelen = strlen(base) - 3;
	cmname = kore_malloc(mnamelen + 1);
	strncpy(cmname, base, mnamelen);
	kore_log(LOG_NOTICE, "loading python module '%s'", cmname);
	mname = PyUnicode_FromString(cmname);
	kore_free(cmname);

	// import the module
	mod = PyImport_Import(mname);
	Py_DECREF(mname);

	if (mod == NULL) {
		if (PyErr_Occurred())
		  PyErr_Print();

		kore_log(LOG_ERR, "failed to load python module from '%s'",
			              path);
		return NULL;
	}

	return mod;
}

PyObject *
kore_pymodule_getfunc(PyObject *pymod, const char* func)
{
	return NULL;
}
