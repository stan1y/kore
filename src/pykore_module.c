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
#include "structmember.h"
#include "http.h"

/* Kore exception type */
static PyObject *PyKoreError; 

/* 
 * Request Object 
 */

static void
pykore_HttpRequest_dealloc(pykore_HttpRequest *self, void *closure)
{
    kore_log(LOG_DEBUG, "%s: req=%p",
                        __FUNCTION__,
                        self->req);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *
pykore_HttpRequest_host(pykore_HttpRequest* self, void *closure)
{
    return PyUnicode_FromString(self->req->host);
}

static PyObject *
pykore_HttpRequest_path(pykore_HttpRequest* self, void *closure)
{
    return PyUnicode_FromString(self->req->path);
}

static PyObject *
pykore_HttpRequest_agent(pykore_HttpRequest* self, void *closure)
{
    return PyUnicode_FromString(self->req->agent);
}

static PyObject *
pykore_HttpRequest_method(pykore_HttpRequest* self, void *closure)
{
    return PyLong_FromLong(self->req->method);
}

static PyObject *
pykore_HttpRequest_body(pykore_HttpRequest* self, void *closure)
{
    PyObject *body;
    struct kore_buf *buf;
    int r;

    buf = kore_buf_alloc(http_body_max);
    r = http_body_read(self->req, buf, sizeof(buf));

    if (r == -1) {
        kore_log(LOG_ERR, "%s: failed to read body",
                          __FUNCTION__);

        // raise python exception
        PyErr_SetString(PyKoreError, "Failed to read request body");
        return NULL;
    }

    body = PyBytes_FromString(kore_buf_stringify(buf, NULL));
    kore_buf_free(buf);

    return body;
}

static PyMethodDef pykore_HttpRequest_methods[] = {
    // FIXME: expose request methods
    {NULL}
};

static PyGetSetDef pykore_HttpRequest_getset[] = {
    {"host",
     (getter)pykore_HttpRequest_host, NULL,
     "Request target host",
     NULL},

    {"path",
     (getter)pykore_HttpRequest_path, NULL,
     "Requested path",
     NULL},

    {"agent",
     (getter)pykore_HttpRequest_agent, NULL,
     "Requesting agent name",
     NULL},    
    
    {"method",
     (getter)pykore_HttpRequest_method, NULL,
     "Request method",
     NULL},

    {"body",
     (getter)pykore_HttpRequest_body, NULL,
     "Request payload",
     NULL},

    {NULL}  /* Sentinel */
};

static PyMemberDef pykore_HttpRequest_members[] = {

    {"req", T_INT, offsetof(pykore_HttpRequest, req), 0,
     "Raw Kore Request object pointer"},

    {NULL}
};

static PyTypeObject pykore_HttpRequestType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "kore.HttpRequest",             /* tp_name */
    sizeof(pykore_HttpRequest), /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)pykore_HttpRequest_dealloc, /* tp_dealloc */
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
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,/* tp_flags */
    "Http Request Class",           /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    pykore_HttpRequest_methods,             /* tp_methods */
    pykore_HttpRequest_members,             /* tp_members */
    pykore_HttpRequest_getset,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,      /* tp_init */
    0,                         /* tp_alloc */
    0,                 /* tp_new */
};

/* Construct instance of HttpRequest for python call */
PyObject *
pykore_httpreq_new(struct http_request *req)
{
    pykore_HttpRequest *pyreq;
    
    kore_log(LOG_DEBUG, "%s: creating request from %p", __FUNCTION__, req);

    pyreq = PyObject_New(pykore_HttpRequest, &pykore_HttpRequestType);
    if (pyreq == NULL) {
        if (PyErr_Occurred())
            PyErr_Print();

        return NULL;
    }

    pyreq->req = req;
    
    return (PyObject*)pyreq;
}

/* 
 * Module level functions
 */

static PyObject*
pykore_http_response_header(PyObject *self, PyObject *args)
{
    PyObject *req, *header, *value;
    pykore_HttpRequest* r;

    if (!PyArg_ParseTuple(args, "OOO", &req, &header, &value))
        return NULL;

    if (!PyObject_IsInstance(req, (PyObject*)&pykore_HttpRequestType)) {
        PyErr_SetString(PyExc_TypeError, "first argument is not a kore.HttpRequest");
        return NULL;
    }

    r = (pykore_HttpRequest*)req;
    http_response_header(
        r->req,
        PyUnicode_AsUTF8(header),
        PyUnicode_AsUTF8(value));

    Py_RETURN_NONE;
}

static PyObject*
pykore_http_response(PyObject *self, PyObject *args)
{
    PyObject *req, *code, *body;
    pykore_HttpRequest* r;

    if (!PyArg_ParseTuple(args, "OIO", &req, &code, &body))
        return NULL;

    if (!PyObject_IsInstance(req, (PyObject*)&pykore_HttpRequestType)) {
        PyErr_SetString(PyExc_TypeError, "first argument is not a kore.HttpRequest");
        return NULL;
    }

    if (!PyBytes_Check(body)) {
        PyErr_SetString(PyExc_TypeError, "body argument must be bytes.");
        return NULL;
    }

    r = (pykore_HttpRequest*)req;
    http_response(
        r->req,
        PyLong_AsLong(code),
        PyBytes_AsString(body),
        PyBytes_Size(body));

    return NULL;
}

static PyMethodDef pykore_methods[] = {

    {"http_response_header", (PyCFunction)pykore_http_response_header, METH_VARARGS,
     "Set response header for given request."},

     {"http_response", (PyCFunction)pykore_http_response, METH_VARARGS,
     "Send response for given request."},
	
    {NULL, NULL, 0, NULL}
};

/*
 * Module initialization
 */

static struct PyModuleDef pykore_module = {
   PyModuleDef_HEAD_INIT,
   "kore",
   NULL,
   -1, /* size of per-interpreter state of the module,
          or -1 if the module keeps state in global variables. */
   pykore_methods
};

PyMODINIT_FUNC
PyInit_kore(void)
{
    PyObject *m;

    // Create types
    pykore_HttpRequestType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&pykore_HttpRequestType) < 0)
        return NULL;

    // Create module
    m = PyModule_Create(&pykore_module);
    if (m == NULL)
        return NULL;

    // Setup exception class
    PyKoreError = PyErr_NewException("kore.error", NULL, NULL);
    Py_INCREF(PyKoreError);
    PyModule_AddObject(m, "error", PyKoreError);

    // Add types to module
    Py_INCREF(&pykore_HttpRequestType);
    PyModule_AddObject(m, "HttpRequest", (PyObject *)&pykore_HttpRequestType);

    return m;
}