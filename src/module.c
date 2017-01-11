/*
 * Copyright (c) 2013-2016 Joris Vink <joris@coders.se>
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

#include <sys/stat.h>

#include <dlfcn.h>

#include "kore.h"

#if defined(KORE_USE_PYTHON)
#include "pykore.h"
#endif


static TAILQ_HEAD(, kore_module)	modules;

void
kore_module_init(void)
{
	TAILQ_INIT(&modules);
}

void
kore_module_cleanup(void)
{
	struct kore_module	*module, *next;

	for (module = TAILQ_FIRST(&modules); module != NULL; module = next) {
		next = TAILQ_NEXT(module, list);
		TAILQ_REMOVE(&modules, module, list);

		kore_free(module->path);

		switch(module->runtime) {
			default:
			case RUNTIME_TYPE_NATIVE:
				(void)dlclose(module->handle);
				break;

#if defined(KORE_USE_PYTHON)
			case RUNTIME_TYPE_PYTHON:
				Py_DECREF(module->handle);
				break;
#endif
		}

		kore_free(module);
	}
}

void
kore_module_load(const char *path, const char *onload)
{
#if !defined(KORE_SINGLE_BINARY)
	struct stat		st;
#endif
	struct kore_module	*module;

	kore_debug("kore_module_load(%s, %s)", path, onload);

	module = kore_malloc(sizeof(struct kore_module));
	module->onload = NULL;
	module->ocb = NULL;

#if !defined(KORE_SINGLE_BINARY)
	if (stat(path, &st) == -1)
		fatal("stat(%s): %s", path, errno_s);

	module->path = kore_strdup(path);
	module->mtime = st.st_mtime;
#else
	module->path = NULL;
	module->mtime = 0;
#endif
	if (path != NULL) {
		/* module can be of different nature */
		if (strstr(module->path, ".so") != NULL) {
			module->runtime = RUNTIME_TYPE_NATIVE;
			module->handle = dlopen(module->path, RTLD_NOW | RTLD_GLOBAL);
			if (module->handle == NULL)
				fatal("%s: %s", path, dlerror());	
		}

#if defined(KORE_USE_PYTHON)
		if (strstr(module->path, ".py") != NULL) {
			module->runtime = RUNTIME_TYPE_PYTHON;
			module->handle = pykore_fload(module->path);
			if (module->handle == NULL)
				fatal("%s: %s", path, "failed to load python module");
		}
#endif
	}
	if (onload != NULL) {
		module->onload = kore_strdup(onload);
		module->ocb = kore_module_getfunc(module, onload);
		if (module->ocb == NULL)
			fatal("%s: onload '%s' not present", path, onload);
	}

	TAILQ_INSERT_TAIL(&modules, module, list);
}

void
kore_module_onload(void)
{
#if !defined(KORE_SINGLE_BINARY)
	struct kore_module	*module;

	TAILQ_FOREACH(module, &modules, list) {
		if (module->ocb == NULL)
			continue;

		(void)kore_module_handler_onload(module, KORE_MODULE_LOAD);
	}
#endif
}

int kore_module_handler_onload(struct kore_module *module, int action)
{
	int rc, (*cb)(int);

	rc = KORE_RESULT_ERROR;
	
	if (module->ocb == NULL)
		return rc;

	switch (module->runtime) {
		default:
		case RUNTIME_TYPE_NATIVE:
			*(void **)&(cb) = module->ocb;
			rc = cb(action);
			break;

#if defined(KORE_USE_PYTHON)
		case RUNTIME_TYPE_PYTHON:
			rc = pykore_handle_onload(module, action);
			break;
#endif

	}

	return rc;
}

void
kore_module_reload(int cbs)
{
#if !defined(KORE_SINGLE_BINARY)
	struct stat			st;
	struct kore_domain		*dom;
	struct kore_module_handle	*hdlr;
	struct kore_module		*module;

	TAILQ_FOREACH(module, &modules, list) {
		if (stat(module->path, &st) == -1) {
			kore_log(LOG_NOTICE, "stat(%s): %s, skipping reload",
			    module->path, errno_s);
			continue;
		}

		if (module->mtime == st.st_mtime)
			continue;

		if (module->ocb != NULL && cbs == 1) {
			if (!kore_module_handler_onload(module, KORE_MODULE_UNLOAD)) {
				kore_log(LOG_NOTICE,
				    "not reloading %s", module->path);
				continue;
			}
		}

		module->mtime = st.st_mtime;
		if (dlclose(module->handle))
			fatal("cannot close existing module: %s", dlerror());

		module->handle = dlopen(module->path, RTLD_NOW | RTLD_GLOBAL);
		if (module->handle == NULL)
			fatal("kore_module_reload(): %s", dlerror());

		if (module->onload != NULL) {
			module->ocb = kore_module_getfunc(module, module->onload);
			if (module->ocb == NULL) {
				fatal("%s: onload '%s' not present",
				    module->path, module->onload);
			}

			if (cbs)
				(void)kore_module_handler_onload(module, KORE_MODULE_LOAD);
		}

		kore_log(LOG_NOTICE, "reloaded '%s' module", module->path);
	}

	TAILQ_FOREACH(dom, &domains, list) {
		TAILQ_FOREACH(hdlr, &(dom->handlers), list) {
			kore_module_findfunc(&hdlr->func, hdlr->fname);
			if (hdlr->func == NULL)
				fatal("no function '%s' found", hdlr->fname);
			hdlr->errors = 0;
		}
	}

#if !defined(KORE_NO_HTTP)
	kore_validator_reload();
#endif
#endif
}

int
kore_module_loaded(void)
{
	if (TAILQ_EMPTY(&modules))
		return (0);

	return (1);
}

#if !defined(KORE_NO_HTTP)
int
kore_module_handler_new(const char *path, const char *domain,
    const char *fname, const char *auth, int type)
{
	int 				runtime;
	void                *func;
	struct kore_auth		*ap;
	struct kore_domain		*dom;
	struct kore_module_handle	*hdlr;

	kore_debug("kore_module_handler_new(%s, %s, %s, %s, %d)", path,
	    domain, fname, auth, type);

	func = NULL;	
	runtime = kore_module_findfunc(&func, fname);
	if (func == NULL) {
		kore_debug("function '%s' not found", fname);
		return (KORE_RESULT_ERROR);
	}
	kore_debug("loaded function '%s' <%p>, runtime=%d",
		       fname, func, runtime);

	if ((dom = kore_domain_lookup(domain)) == NULL)
		return (KORE_RESULT_ERROR);

	if (auth != NULL) {
		if ((ap = kore_auth_lookup(auth)) == NULL)
			fatal("no authentication block '%s' found", auth);
	} else {
		ap = NULL;
	}

	hdlr = kore_malloc(sizeof(*hdlr));
	hdlr->auth = ap;
	hdlr->dom = dom;
	hdlr->errors = 0;
	hdlr->func = func;
	hdlr->type = type;
	hdlr->runtime = runtime;
	TAILQ_INIT(&(hdlr->params));
	hdlr->path = kore_strdup(path);
	hdlr->fname = kore_strdup(fname);

	if (hdlr->type == HANDLER_TYPE_DYNAMIC) {
		if (regcomp(&(hdlr->rctx), hdlr->path,
		    REG_EXTENDED | REG_NOSUB)) {
			kore_module_handler_free(hdlr);
			kore_debug("regcomp() on %s failed", path);
			return (KORE_RESULT_ERROR);
		}
	}

	TAILQ_INSERT_TAIL(&(dom->handlers), hdlr, list);
	return (KORE_RESULT_OK);
}

void
kore_module_handler_free(struct kore_module_handle *hdlr)
{
	struct kore_handler_params *param;

	if (hdlr == NULL)
		return;


	if (hdlr->fname != NULL)
		kore_free(hdlr->fname);


	if (hdlr->func != NULL)
		kore_module_handler_reset(hdlr);


	if (hdlr->path != NULL)
		kore_free(hdlr->path);
	if (hdlr->type == HANDLER_TYPE_DYNAMIC)
		regfree(&(hdlr->rctx));

	/* Drop all validators associated with this handler */
	while ((param = TAILQ_FIRST(&(hdlr->params))) != NULL) {
		TAILQ_REMOVE(&(hdlr->params), param, list);
		if (param->name != NULL)
			kore_free(param->name);
		kore_free(param);
	}

	kore_free(hdlr);
}

struct kore_module_handle *
kore_module_handler_find(const char *domain, const char *path)
{
	struct kore_domain		*dom;
	struct kore_module_handle	*hdlr;

	if ((dom = kore_domain_lookup(domain)) == NULL)
		return (NULL);

	TAILQ_FOREACH(hdlr, &(dom->handlers), list) {
		if (hdlr->type == HANDLER_TYPE_STATIC) {
			if (!strcmp(hdlr->path, path))
				return (hdlr);
		} else {
			if (!regexec(&(hdlr->rctx), path, 0, NULL, 0))
				return (hdlr);
		}
	}

	return (NULL);
}

#endif /* !KORE_NO_HTTP */

void *
kore_module_getsym(const char* symbol)
{
	struct kore_module	*module;
	void                *ptr;

	TAILQ_FOREACH(module, &modules, list) {
		ptr = dlsym(module->handle, symbol);
		if (ptr != NULL)
			return ptr;
	}

	return NULL;
}

void *
kore_module_getfunc(struct kore_module *module, const char *symbol)
{
	void                *ptr;
	
	ptr = NULL;
	switch (module->runtime) {
		default:
		case RUNTIME_TYPE_NATIVE:
			ptr = dlsym(module->handle, symbol);
			break;

#if defined(KORE_USE_PYTHON)
		case RUNTIME_TYPE_PYTHON:
			ptr = pykore_getclb(
				(PyObject*)module->handle, symbol);
			break;
#endif
	}

	return ptr;	
}

int
kore_module_findfunc(void **out, const char *symbol)
{
	struct kore_module	*module;
	void                *ptr;

	ptr = NULL;
	TAILQ_FOREACH(module, &modules, list) {
		ptr = kore_module_getfunc(module, symbol);
		if (ptr != NULL) {
			*out = ptr;
			return module->runtime;
		}
	}

	return KORE_RESULT_ERROR;
}

void
kore_module_handler_reset(struct kore_module_handle *hdlr)
{
	if (hdlr == NULL)
		return;

	/* runtime specific cleanup of handler func */
	switch(hdlr->runtime) {

		default:
		case RUNTIME_TYPE_NATIVE:
			hdlr->func = NULL;
			break;

#if defined(KORE_USE_PYTHON)

		case RUNTIME_TYPE_PYTHON:
			Py_DECREF((PyObject*)hdlr->func);
			hdlr->func = NULL;
			break;
#endif
	}
}
