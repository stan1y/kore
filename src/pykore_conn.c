/*
 * Copyright (c) 2017 Stanislav Yudin <stan@endlessinsomnia.com>
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
 * Connection object implementation
 */

typedef struct {
	PyObject_HEAD
	struct connection *op_conn;

} Connection;

static PyObject *
Connection_type(Connection* self, void *closure)
{
	return PyLong_FromUnsignedLong(self->op_conn->type);
}

static PyObject *
Connection_addr(Connection* self, void *closure)
{
	PyObject	*addr;
	char 		 saddr[INET6_ADDRSTRLEN];

	memset(saddr, 0, sizeof(saddr));
	if (self->op_conn->addrtype == AF_INET) {
		inet_ntop(AF_INET, &self->op_conn->addr.ipv4.sin_addr, saddr, sizeof(saddr));
	}
	if (self->op_conn->addrtype == AF_INET6) {
		inet_ntop(AF_INET6, &self->op_conn->addr.ipv6.sin6_addr, saddr, sizeof(saddr));
	}
	addr = PyUnicode_FromString(saddr);
	return addr;
}

static PyGetSetDef Connection_getset[] = {
	{"type",
	 (getter)Connection_type, NULL,
	 "Connection Type",
	 NULL},

	{"addr",
	 (getter)Connection_addr, NULL,
	 "Client IP address",
	 NULL},

	{NULL}
};

static PyMethodDef Connection_methods[] = {
	{NULL}
};

static PyMemberDef Connection_members[] = {
	{NULL}
};

static PyTypeObject ConnectionType = {
	PyVarObject_HEAD_INIT(NULL, 0)
    "kore.Connection",             /* tp_name */
    sizeof(Connection),             /* tp_basicsize */
    0,                         /* tp_itemsize */
    0,							/* tp_dealloc */
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
    "struct connection",           /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    Connection_methods,             /* tp_methods */
    Connection_members,             /* tp_members */
    Connection_getset,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,      /* tp_init */
    0,                         /* tp_alloc */
    PyType_GenericNew,         /* tp_new */
};

void
pykore_connection_register(PyObject *module)
{
    if (PyType_Ready(&ConnectionType) < 0) {
        fatal("python: failed to initialize Connection type");
        return;
    }
    Py_INCREF(&ConnectionType);
    if(PyModule_AddObject(module, "Connection", (PyObject *) &ConnectionType) < 0) {
    	fatal("python: failed to add Connection type");
        return;
    }
}

int
pykore_connection_check(PyObject* o)
{
	return PyObject_IsInstance(o, (PyObject*)&ConnectionType);
}

PyObject *
pykore_connection_create(struct connection *conn)
{
	Connection	*pyconn;
	pyconn = PyObject_New(Connection, &ConnectionType);
	if (pyconn == NULL) {
		if (PyErr_Occurred())
			PyErr_Print();

		return NULL;
	}

	pyconn->op_conn = conn;
	return (PyObject*)pyconn;
}