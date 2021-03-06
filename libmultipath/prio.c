#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <dlfcn.h>
#include <sys/stat.h>

#include "debug.h"
#include "prio.h"
#include "config.h"

static LIST_HEAD(prioritizers);

int init_prio (void)
{
	if (!add_prio(DEFAULT_PRIO))
		return 1;
	return 0;
}

static struct prio * alloc_prio (void)
{
	struct prio *p;

	p = MALLOC(sizeof(struct prio));
	if (p)
		INIT_LIST_HEAD(&p->node);
	return p;
}

void free_prio (struct prio * p)
{
	condlog(3, "unloading %s prioritizer", p->name);
	list_del(&p->node);
	if (p->handle) {
		if (dlclose(p->handle) != 0) {
			condlog(0, "Cannot unload prioritizer %s: %s",
				p->name, dlerror());
		}
	}
	FREE(p);
}

void cleanup_prio(void)
{
	struct prio * prio_loop;
	struct prio * prio_temp;

	list_for_each_entry_safe(prio_loop, prio_temp, &prioritizers, node) {
		free_prio(prio_loop);
	}
}

struct prio * prio_lookup (char * name)
{
	struct prio * p;

	list_for_each_entry(p, &prioritizers, node) {
		if (!strncmp(name, p->name, PRIO_NAME_LEN))
			return p;
	}
	return add_prio(name);
}

int prio_set_args (struct prio * p, char * args)
{
	return snprintf(p->args, PRIO_ARGS_LEN, "%s", args);
}

struct prio * add_prio (char * name)
{
	char libname[LIB_PRIO_NAMELEN];
	struct stat stbuf;
	struct prio * p;
	char *errstr;

	p = alloc_prio();
	if (!p)
		return NULL;
	snprintf(libname, LIB_PRIO_NAMELEN, "%s/libprio%s.so",
		 conf->multipath_dir, name);
	if (stat(libname,&stbuf) < 0) {
		condlog(0,"Prioritizer '%s' not found in %s",
			name, conf->multipath_dir);
		goto out;
	}
	condlog(3, "loading %s prioritizer", libname);
	p->handle = dlopen(libname, RTLD_NOW);
	if (!p->handle) {
		if ((errstr = dlerror()) != NULL)
			condlog(0, "A dynamic linking error occurred: (%s)",
				errstr);
		goto out;
	}
	p->getprio = (int (*)(struct path *, char *)) dlsym(p->handle, "getprio");
	errstr = dlerror();
	if (errstr != NULL)
		condlog(0, "A dynamic linking error occurred: (%s)", errstr);
	if (!p->getprio)
		goto out;
	snprintf(p->name, PRIO_NAME_LEN, "%s", name);
	list_add(&p->node, &prioritizers);
	return p;
out:
	free_prio(p);
	return NULL;
}

int prio_getprio (struct prio * p, struct path * pp)
{
	return p->getprio(pp, p->args);
}

char * prio_name (struct prio * p)
{
	return p->name;
}

char * prio_args (struct prio * p)
{
	return p->args;
}
