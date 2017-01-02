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
#include "sys/param.h"

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

int
kore_python_init(void)
{
	wchar_t *whome;
	PyObject *kore_module, *kore, *sysmodules;

	if (python_home != NULL && strlen(python_home) > 0) {
		kore_log(LOG_DEBUG, "python home is '%s'", python_home);
		whome = kore_get_wcstr(python_home);
		Py_SetPythonHome(whome);
		kore_free(whome);
	}

  	Py_SetStandardStreamEncoding("utf-8", "utf-8");
	Py_Initialize();
	
	kore_module = PyInit_kore();
	if (kore_module == NULL) {
		if (PyErr_Occurred())
			PyErr_Print();

		kore_log(LOG_ERR, "%s: failed to init python interface",
			              __FUNCTION__);
		return (KORE_RESULT_ERROR);
	}

	sysmodules = PySys_GetObject("modules");
	if (sysmodules == NULL) {
		Py_DECREF(kore_module);

		if (PyErr_Occurred())
			PyErr_Print();

		kore_log(LOG_ERR, "%s: failed to setup python interface",
			              __FUNCTION__);
		return (KORE_RESULT_ERROR);	
	}

	kore = PyUnicode_FromString("kore");
	PyDict_SetItem(sysmodules, kore, kore_module);
	Py_DECREF(kore);
	Py_DECREF(kore_module);
	Py_DECREF(sysmodules);

	kore_log(LOG_NOTICE, "initialized python runtime");
	return (KORE_RESULT_OK);
}

void
kore_python_cleanup(void)
{
	if (Py_IsInitialized())
		Py_Finalize();
}

void
pykore_printver()
{
	// FIXME: report python version here
	printf("python ");
}

/* Load python module from a file */
PyObject *
pykore_fload(char *path)
{
	char           *p, *dname, *base, *cmname;
	PyObject       *mod, *mname;
	size_t         mnamelen;

	if (!Py_IsInitialized())
		kore_python_init();

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
	mname = PyUnicode_FromString(cmname);

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
	kore_free(cmname);

	Py_DECREF(mod);
	return mod;
}

/* Lookup object arribute by name */
PyObject *
pykore_getclb(PyObject *pymod, const char* fname)
{
	PyObject *attr;

	attr = PyObject_GetAttrString(pymod, fname);
	if (attr == NULL) {
		return NULL;
	}

	if (!PyCallable_Check(attr)) {
    	Py_XDECREF(attr);
    	return NULL;
    }

    Py_DECREF(attr);
	return attr;
}

int
pykore_handle_httpreq(struct http_request *req)
{
	PyObject *pyreq, *args, *kwargs, *ret;

	pyreq = pykore_httpreq_new(req);
	if (pyreq == NULL) {
		return KORE_RESULT_ERROR;
	}

	kwargs = PyDict_New();
	args = PyTuple_New(1);
	PyTuple_SetItem(args, 0, pyreq);

	ret = PyObject_Call(
		(PyObject*)req->hdlr->func, args, kwargs);
	Py_DECREF(pyreq);
	Py_DECREF(args);
	Py_DECREF(kwargs);

	if (ret == NULL) {
		if (PyErr_Occurred())
			PyErr_Print();
		
		return (KORE_RESULT_ERROR);
	}

	return KORE_RESULT_OK;
}