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

/*
 * This file implements glue between kore and python allowing to call
 * into python callbacks and pass arguments as objects
 */

#include <sys/param.h>
#include <libgen.h>
#include "pykore.h"

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

	if (python_home != NULL && strlen(python_home) > 0) {
		kore_log(LOG_DEBUG, "python home is '%s'", python_home);
		whome = kore_get_wcstr(python_home);
		Py_SetPythonHome(whome);
		kore_free(whome);
	}
#if PY_MINOR_VERSION >= 4
	Py_SetStandardStreamEncoding("utf-8", "utf-8");
#endif
	PyImport_AppendInittab("kore", &PyInit_kore);
	Py_Initialize();
	
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
	PyObject	*attr;

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
pykore_returncall(PyObject *ret)
{
	int		rc = KORE_RESULT_OK;

	if (ret == NULL) {
		rc = KORE_RESULT_ERROR;
		if (PyErr_Occurred()) {
			PyErr_Print();
		}
		kore_log(LOG_ERR, "%s: exception occured.", __FUNCTION__);
		return rc;
	}

	if (PyLong_Check(ret)) {
		rc = PyLong_AsLong(ret);
	}
	Py_DECREF(ret);
	
	return rc;
}

int
pykore_handle_httpreq(struct http_request *req)
{
	PyObject	*pyreq, *args, *kwargs, *ret;

	pyreq = pykore_httpreq_create(req);
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

	return pykore_returncall(ret);
}

int
pykore_handle_onload(struct kore_module *module, int action)
{
	PyObject	*pyact, *args, *kwargs, *ret;

	pyact = PyLong_FromLong(action);
	kwargs = PyDict_New();
	args = PyTuple_New(1);
	PyTuple_SetItem(args, 0, pyact);
	ret = PyObject_Call(
		(PyObject*)module->ocb, args, kwargs);
	Py_DECREF(pyact);
	Py_DECREF(args);
	Py_DECREF(kwargs);

	return pykore_returncall(ret);
}

int
pykore_handle_validator(struct kore_validator *val,
	struct http_request *req,
	char *data)
{
	PyObject	*pydata, *pyreq, *args, *kwargs, *ret;

	pyreq = pykore_httpreq_create(req);
	pydata = PyBytes_FromString(data);
	kwargs = PyDict_New();
	args = PyTuple_New(2);
	PyTuple_SetItem(args, 0, pyreq);
	PyTuple_SetItem(args, 1, pydata);
	ret = PyObject_Call(
		(PyObject*)val->func, args, kwargs);
	Py_DECREF(pydata);
	Py_DECREF(pyreq);
	Py_DECREF(args);
	Py_DECREF(kwargs);
	return pykore_returncall(ret);
}

/* WebSockets interface */

struct pykore_wsclb {
	PyObject	*connect;
	PyObject	*disconnect;
	PyObject	*message;
};

static void
pykore_wsconnect(struct connection *c)
{
	struct pykore_wsclb		*clb;
	PyObject				*args, *kwargs, *ret, *conn;

	clb = (struct pykore_wsclb *)c->hdlr_extra;
	conn = pykore_connection_create(c);
	kwargs = PyDict_New();
	args = PyTuple_New(1);
	PyTuple_SetItem(args, 0, conn);
	ret = PyObject_Call(clb->connect, args, kwargs);
	if (ret == NULL) {
		if (PyErr_Occurred()) {
			PyErr_Print();
		}
	}
	Py_DECREF(conn);
	Py_DECREF(args);
	Py_DECREF(kwargs);
}

static void
pykore_wsdisconnect(struct connection *c)
{
	struct pykore_wsclb		*clb;
	PyObject				*args, *kwargs, *ret, *conn;

	clb = (struct pykore_wsclb *)c->hdlr_extra;
	conn = pykore_connection_create(c);
	kwargs = PyDict_New();
	args = PyTuple_New(1);
	PyTuple_SetItem(args, 0, conn);
	ret = PyObject_Call(clb->disconnect, args, kwargs);
	if (ret == NULL) {
		if (PyErr_Occurred()) {
			PyErr_Print();
		}
	}
	Py_DECREF(conn);
	Py_DECREF(args);
	Py_DECREF(kwargs);
}

static void
pykore_wsmessage(struct connection *c,
		    u_int8_t op, void *data, size_t len)
{
	struct pykore_wsclb		*clb;
	PyObject				*args, *kwargs, *ret;
	PyObject				*conn, *pyop, *pydata;

	clb = (struct pykore_wsclb *)c->hdlr_extra;
	conn = pykore_connection_create(c);
	pyop = PyLong_FromUnsignedLong(op);
	pydata = PyBytes_FromString(data);
	kwargs = PyDict_New();
	args = PyTuple_New(3);
	PyTuple_SetItem(args, 0, conn);
	PyTuple_SetItem(args, 1, pyop);
	PyTuple_SetItem(args, 2, pydata);
	ret = PyObject_Call(clb->message, args, kwargs);
	if (ret == NULL) {
		if (PyErr_Occurred()) {
			PyErr_Print();
		}
	}
	Py_DECREF(conn);
	Py_DECREF(pyop);
	Py_DECREF(pydata);
	Py_DECREF(args);
	Py_DECREF(kwargs);

}

static struct kore_wscbs wscbs = {
	pykore_wsconnect,
	pykore_wsmessage,
	pykore_wsdisconnect
};

int 
pykore_websocket_handshake(struct http_request *req,
	PyObject *connect, PyObject *disconnect, PyObject *message)
{
	struct pykore_wsclb		 *pywscbs;

	pywscbs = kore_malloc(sizeof(struct pykore_wsclb));
	memset(pywscbs, 0, sizeof(struct pykore_wsclb));
	pywscbs->connect = connect;
	pywscbs->disconnect = disconnect;
	pywscbs->message = message;
	if (req->owner->hdlr_extra != NULL) {
		kore_log(LOG_ERR, "%s: connection's hdrl_extra already used.",
			__FUNCTION__);
		return (KORE_RESULT_ERROR);
	}
	req->owner->hdlr_extra = (void*)pywscbs;
	kore_websocket_handshake(req, &wscbs);
	return (KORE_RESULT_OK);
}