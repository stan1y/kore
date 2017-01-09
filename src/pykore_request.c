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
#include "http.h"

/* 
 * HttpRequest Object
 * provides access to 'struct http_request' 
 */

typedef struct {
	PyObject_HEAD

	struct http_request *op_req;
	PyObject			*op_body;

} HttpRequest;

static void
HttpRequest_dealloc(HttpRequest *self)
{
	if (self->op_body != NULL && self->op_body != Py_None){
		Py_DECREF(self->op_body);
	}
}

static PyObject *
HttpRequest_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    HttpRequest *self;

    self = (HttpRequest *)type->tp_alloc(type, 0);
    if (self == NULL)
    	return NULL;
    
    self->op_req = NULL;
    self->op_body = NULL;

    return (PyObject *)self;
}

static PyObject *
HttpRequest_host(HttpRequest *self, void *closure)
{
	if (self->op_req->host != NULL)
		return PyUnicode_FromString(self->op_req->host);

	Py_RETURN_NONE;
}

static PyObject *
HttpRequest_path(HttpRequest *self, void *closure)
{
	if (self->op_req->path != NULL)
		return PyUnicode_FromString(self->op_req->path);

	Py_RETURN_NONE;
}

static PyObject *
HttpRequest_agent(HttpRequest *self, void *closure)
{
	if (self->op_req->agent != NULL)
		return PyUnicode_FromString(self->op_req->agent);

	Py_RETURN_NONE;
}

static PyObject *
HttpRequest_method(HttpRequest* self, void *closure)
{
	return PyLong_FromUnsignedLong(self->op_req->method);
}

static PyObject *
HttpRequest_body(HttpRequest* self, void *closure)
{
	struct kore_buf	*buf;
	char			 data[BUFSIZ];
	int				 r;

	if (self->op_body != NULL) {
		Py_INCREF(self->op_body);
		return self->op_body;
	}

	buf = kore_buf_alloc(http_body_max);
	for (;;) {
		r = http_body_read(self->op_req, data, sizeof(data));
		if (r == -1) {
			kore_buf_free(buf);
			/* report error as python exception */
			PyErr_SetString(PyExc_IOError, "Failed to read request body");
			return NULL;
		}
		if (r == 0)
			break;
		kore_buf_append(buf, data, r);
	}

	if (buf->offset == 0) {
		/* Empty body stored as ref to Py_None */
		self->op_body = Py_None;
		Py_INCREF(self->op_body);
	}
	else {
		self->op_body = PyBytes_FromString(kore_buf_stringify(buf, NULL));
	}
	kore_buf_free(buf);
	
	Py_INCREF(self->op_body);
	return self->op_body;
}

static PyObject *
HttpRequest_bodyfd(HttpRequest *self, void *closure)
{
	return PyLong_FromLong(self->op_req->http_body_fd);
}

static PyGetSetDef HttpRequest_getset[] = {
	{"host",
	 (getter)HttpRequest_host, NULL,
	 "Request target host",
	 NULL},

	{"path",
	 (getter)HttpRequest_path, NULL,
	 "Requested path",
	 NULL},

	{"agent",
	 (getter)HttpRequest_agent, NULL,
	 "Requesting agent name",
	 NULL},    
	
	{"method",
	 (getter)HttpRequest_method, NULL,
	 "Request method",
	 NULL},

	{"body",
	 (getter)HttpRequest_body, NULL,
	 "Request payload",
	 NULL},

	{"bodyfd",
	 (getter)HttpRequest_bodyfd, NULL,
	 "Body payload file descriptor",
	 NULL},

	{NULL}
};

static PyObject *
HttpRequest_populate_get(HttpRequest *self)
{
	size_t	count;

	count = http_populate_get(self->op_req);
	return PyLong_FromSize_t(count);
}

static PyObject *
HttpRequest_populate_post(HttpRequest *self)
{
	size_t	count;
	
	count = http_populate_post(self->op_req);
	return PyLong_FromSize_t(count);
}

static PyObject *
HttpRequest_populate_multipart_form(HttpRequest *self)
{
	size_t count;

	count = http_populate_multipart_form(self->op_req);
	return PyLong_FromSize_t(count);
}

static PyObject *
HttpRequest_get_string(HttpRequest *self, PyObject *args)
{
	char				*val;
	const char			*name;

	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;

	if (!http_argument_get_string(self->op_req, name, &val)) {
		Py_RETURN_NONE;
	}

	return PyUnicode_FromString(val);
}

static PyObject *
HttpRequest_get_int(HttpRequest *self, PyObject *args)
{
	int64_t				 val;
	const char			*name;

	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;

	if (!http_argument_get_int64(self->op_req, name, &val)) {
		Py_RETURN_NONE;
	}

	return PyLong_FromLong(val);
}

static PyObject *
HttpRequest_get_uint(HttpRequest *self, PyObject *args)
{
	uint64_t			 val;
	const char			*name;

	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;

	if (!http_argument_get_uint64(self->op_req, name, &val)) {
		Py_RETURN_NONE;
	}

	return PyLong_FromUnsignedLong(val);
}

static PyObject *
HttpRequest_get_file(HttpRequest *self, PyObject *args)
{
	struct http_file	*file;
	const char			*name;

	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;

	file = http_file_lookup(self->op_req, name);
	if (file == NULL)
		Py_RETURN_NONE;
	
	return pykore_httpfile_create(file);
}


static PyObject*
HttpRequest_response_header(HttpRequest *self, PyObject *args)
{
	const char	*header, *value;

	if (!PyArg_ParseTuple(args, "ss", &header, &value))
		return NULL;

	http_response_header(self->op_req, header, value);
	return (PyObject*)self;
}

static PyObject*
HttpRequest_response(HttpRequest *self, PyObject *args)
{
	unsigned int code;
	Py_buffer body;

	if (!PyArg_ParseTuple(args, "Iy*", &code, &body))
		return NULL;

	http_response(
		self->op_req,
		code,
		body.buf,
		body.len);

	return (PyObject*)self;
}

static PyMethodDef HttpRequest_methods[] = {

	{"response_header",
	 (PyCFunction)HttpRequest_response_header,
	 METH_VARARGS, 
	 "Set response header for this request"},

	{"response",
	 (PyCFunction)HttpRequest_response,
	 METH_VARARGS, 
	 "Set response code and payload for this request"},
	
	{"populate_multipart_form",
	 (PyCFunction)HttpRequest_populate_multipart_form,
	 METH_NOARGS, 
	 "Read form/multi-part arguments from request"},

	{"populate_get",
	 (PyCFunction)HttpRequest_populate_get,
	 METH_NOARGS, 
	 "Read GET arguments from request"},

	{"populate_post",
	 (PyCFunction)HttpRequest_populate_post,
	 METH_NOARGS, 
	 "Read POST arguments from request"},

	{"get_string",
	 (PyCFunction)HttpRequest_get_string,
	 METH_VARARGS, 
	 "Lookup string argument in request by name"},

	{"get_int",
	 (PyCFunction)HttpRequest_get_int,
	 METH_VARARGS, 
	 "Lookup integer argument in request by name"},

	{"get_uint",
	 (PyCFunction)HttpRequest_get_uint,
	 METH_VARARGS, 
	 "Lookup integer argument in request by name"},

	{"get_file",
	 (PyCFunction)HttpRequest_get_file,
	 METH_VARARGS, 
	 "Lookup an uploaded file in request by name"},

	{NULL}
};

static PyMemberDef HttpRequest_members[] = {

	{"req", T_INT, offsetof(HttpRequest, op_req), 0,
	 "Raw struct http_request object pointer"},

	{NULL}
};

static PyTypeObject HttpRequestType = {
	PyVarObject_HEAD_INIT(NULL, 0)
    "kore.HttpRequest",             /* tp_name */
    sizeof(HttpRequest),             /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)HttpRequest_dealloc, /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_reserved */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash  */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT |
    Py_TPFLAGS_BASETYPE,   /* tp_flags */
    "struct http_request",           /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    HttpRequest_methods,             /* tp_methods */
    HttpRequest_members,             /* tp_members */
    HttpRequest_getset,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,      /* tp_init */
    0,                         /* tp_alloc */
    HttpRequest_new,         /* tp_new */
};

void
pykore_httpreq_register(PyObject *module)
{
    if (PyType_Ready(&HttpRequestType) < 0) {
        fatal("python: failed to initialize HttpRequest");
        return;
    }
    Py_INCREF(&HttpRequestType);
    if(PyModule_AddObject(module, "HttpRequest", (PyObject *) &HttpRequestType) < 0) {
    	fatal("python: failed to add HttpRequest");
        return;
    }
}

int
pykore_httpreq_check(PyObject* o)
{
	return PyObject_IsInstance(o, (PyObject*)&HttpRequestType);
}

PyObject *
pykore_httpreq_create(struct http_request *req)
{
	HttpRequest	*pyreq;

	pyreq = PyObject_New(HttpRequest, &HttpRequestType);
	if (pyreq == NULL) {
		if (PyErr_Occurred())
			PyErr_Print();

		return NULL;
	}

	pyreq->op_req = req;
	pyreq->op_body = NULL;
	Py_INCREF(pyreq);

	return (PyObject*)pyreq;
}