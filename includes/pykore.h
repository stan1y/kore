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

#ifndef __H_PYKORE_H
#define __H_PYKORE_H

#include <Python.h>
#include <structmember.h>
#include "kore.h"
#include "http.h"

/* Config setting storage */
extern char		*python_home;

int			 kore_python_init(void);
void		 kore_python_cleanup(void);
void		 pykore_printver(void);
int			 pykore_returncall(PyObject *);

PyObject	*pykore_fload(char *);
PyObject	*pykore_getclb(PyObject *, const char*);

int			 pykore_handle_httpreq(struct http_request *);
int			 pykore_handle_onload(struct kore_module *, int);
int			 pykore_handle_validator(struct kore_validator *, struct http_request *, char *);

int			 pykore_websocket_handshake(struct http_request *, PyObject *, PyObject *, PyObject *);

void		 pykore_httpreq_register(PyObject *);
PyObject	*pykore_httpreq_create(struct http_request *);
int			 pykore_httpreq_check(PyObject *);
void		 pykore_httpfile_register(PyObject *);
PyObject	*pykore_httpfile_create(struct http_file *);
int			 pykore_httpfile_check(PyObject *);


PyMODINIT_FUNC
PyInit_kore(void);

#endif //__H_PYKORE_H