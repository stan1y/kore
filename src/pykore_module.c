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
 * Python 'kore' module implementation
 */

#include "pykore.h"
#include "http.h"

#define PYCONST(m, name, sym) \
    do { \
        PyObject *io = PyLong_FromLong(sym); \
        PyModule_AddObject((m), (name), io); \
        Py_DECREF(io); \
    } while(0)

/* Module level functions */
static PyMethodDef pykore_methods[] = {
	{NULL, NULL, 0, NULL}
};

/* Module initialization */
static struct PyModuleDef pykore_module = {
   PyModuleDef_HEAD_INIT,
   "kore",
   NULL,
   -1, /* size of per-interpreter state of the module,
		* or -1 if the module keeps state in global variables.
		*/
   pykore_methods
};

PyMODINIT_FUNC
PyInit_kore(void)
{
	PyObject *m;

	m = PyModule_Create(&pykore_module);
	if (m == NULL) {
		fatal("python: failed to initialize 'kore' module.");
		return NULL;
	}

	pykore_httpreq_register(m);
	pykore_httpfile_register(m);

	PYCONST(m, "RESULT_OK", KORE_RESULT_OK);
	PYCONST(m, "RESULT_ERROR", KORE_RESULT_ERROR);
	PYCONST(m, "MODULE_LOAD", KORE_MODULE_LOAD);
	PYCONST(m, "MODULE_UNLOAD", KORE_MODULE_UNLOAD);
	PYCONST(m, "METHOD_GET", HTTP_METHOD_GET);
	PYCONST(m, "METHOD_POST", HTTP_METHOD_POST);
	PYCONST(m, "METHOD_PUT", HTTP_METHOD_PUT);
	PYCONST(m, "METHOD_DELETE", HTTP_METHOD_DELETE);
	PYCONST(m, "METHOD_HEAD", HTTP_METHOD_HEAD);
	PYCONST(m, "WEBSOCKET_BROADCAST_GLOBAL", WEBSOCKET_BROADCAST_GLOBAL);

	return m;
}
