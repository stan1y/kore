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
 * HttpFile Object
 * provides access to 'struct http_file'
 */

typedef struct {
	PyObject_HEAD
	struct http_file *op_file;

} HttpFile;

static PyObject *
HttpFile_name(HttpFile* self, void *closure)
{
	if (self->op_file->name != NULL)
		return PyUnicode_FromString(self->op_file->name);

	Py_RETURN_NONE;
}

static PyObject *
HttpFile_filename(HttpFile* self, void *closure)
{
	if (self->op_file->filename != NULL)
		return PyUnicode_FromString(self->op_file->filename);

	Py_RETURN_NONE;
}

static PyObject *
HttpFile_position(HttpFile* self, void *closure)
{
	return PyLong_FromSize_t(self->op_file->position);
}

static PyObject *
HttpFile_offset(HttpFile* self, void *closure)
{
	return PyLong_FromSize_t(self->op_file->offset);
}

static PyObject *
HttpFile_length(HttpFile* self, void *closure)
{
	return PyLong_FromSize_t(self->op_file->length);
}

static PyGetSetDef HttpFile_getset[] = {
	{"name",
	 (getter)HttpFile_name, NULL,
	 "File argument name",
	 NULL},

	{"filename",
	 (getter)HttpFile_filename, NULL,
	 "Filename",
	 NULL},

	{"position",
	 (getter)HttpFile_position, NULL,
	 "Current read position",
	 NULL},

	{"offset",
	 (getter)HttpFile_offset, NULL,
	 "File offset",
	 NULL},

	{"length",
	 (getter)HttpFile_length, NULL,
	 "File length in bytes",
	 NULL},

	{NULL}
};

static PyObject*
HttpFile_read(HttpFile *self, PyObject *args)
{
	Py_ssize_t	 n;
	char		*buf;
	PyObject	*pybuf;

	if (!PyArg_ParseTuple(args, "n", &n))
		return NULL;

	buf = kore_malloc(n);
	if (!http_file_read(self->op_file, buf, n)) {
		kore_free(buf);
		/* report error as python exception */
		PyErr_SetString(PyExc_IOError, "Failed to read file contents.");
		return NULL;
	}

	pybuf = PyBytes_FromString(buf);
	kore_free(buf);

	return pybuf;
}

static PyObject*
HttpFile_readall(HttpFile *self)
{
	int					 r;
	char				 data[BUFSIZ];
	struct kore_buf		*buf;
	PyObject			*pybuf;

	http_file_rewind(self->op_file);
	buf = kore_buf_alloc(http_body_max);
	for (;;) {
		r = http_file_read(self->op_file, data, sizeof(data));
		if (r == -1) {
			kore_buf_free(buf);
			/* report error as python exception */
			PyErr_SetString(PyExc_IOError, "Failed to read file content");
			return NULL;
		}
		if (r == 0)
			break;
		kore_buf_append(buf, data, r);
	}

	if (buf->offset == 0) {
		/* Empty read -> None */
		pybuf = Py_None;
		Py_INCREF(pybuf);
	}
	else {
		pybuf = PyBytes_FromString(kore_buf_stringify(buf, NULL));
	}

	kore_buf_free(buf);
	return pybuf;
}

static PyObject*
HttpFile_rewind(HttpFile *self)
{
	http_file_rewind(self->op_file);
	return (PyObject *)self;
}

static PyMethodDef HttpFile_methods[] = {
	{"read",
	 (PyCFunction)HttpFile_read,
	 METH_VARARGS, 
	 "Read given number of bytes from this file."},

	{"readall",
	 (PyCFunction)HttpFile_readall,
	 METH_NOARGS, 
	 "Read everything from this file."},

	{"rewind",
	 (PyCFunction)HttpFile_rewind,
	 METH_NOARGS, 
	 "Reset state of this file."},

	{NULL}
};

static PyMemberDef HttpFile_members[] = {
	{NULL}
};

static PyTypeObject HttpFileType = {
	PyVarObject_HEAD_INIT(NULL, 0)
    "kore.HttpFile",             /* tp_name */
    sizeof(HttpFile),             /* tp_basicsize */
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
    "struct http_file",           /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    HttpFile_methods,             /* tp_methods */
    HttpFile_members,             /* tp_members */
    HttpFile_getset,                         /* tp_getset */
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
pykore_httpfile_register(PyObject *module)
{
    if (PyType_Ready(&HttpFileType) < 0) {
        fatal("python: failed to initialize HttpFile");
        return;
    }
    Py_INCREF(&HttpFileType);
    if(PyModule_AddObject(module, "HttpFile", (PyObject *) &HttpFileType) < 0) {
    	fatal("python: failed to add File");
        return;
    }
}

int
pykore_httpfile_check(PyObject* o)
{
	return PyObject_IsInstance(o, (PyObject*)&HttpFileType);
}

PyObject *
pykore_httpfile_create(struct http_file *file)
{
	HttpFile	*pyfile;
	pyfile = PyObject_New(HttpFile, &HttpFileType);
	if (pyfile == NULL) {
		if (PyErr_Occurred())
			PyErr_Print();

		return NULL;
	}

	pyfile->op_file = file;
	return (PyObject*)pyfile;
}