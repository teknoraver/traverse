#define _GNU_SOURCE
#include "traverse.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <ctype.h>

/* Transfer ownership of a pointer out of a _cleanup_free_ variable */
#define TAKE_PTR(p)			\
	({				\
		__typeof__(p) _p = (p);	\
		(p) = NULL;		\
		_p;			\
	})

/* Transfer ownership of an fd out of a _cleanup_close_ variable */
#define TAKE_FD(fd)			\
	({				\
		int _fd = (fd);		\
		(fd) = -1;		\
		_fd;			\
	})

/* linux_dirent64 layout for getdents64 */
struct linux_dirent64 {
	ino64_t d_ino;
	off64_t d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[];
};

struct dir_stack {
	int fd;
	char *path;
	struct dir_stack *next;
	struct traverse_ctx_entry *entries;
	size_t count;
	size_t index;
};

struct traverse_ctx {
	int flags;
	struct dir_stack *stack;
};

/* cleanup helpers */
static inline void cleanup_free(void *p)
{
	void **pp = (void **)p;
	free(*pp);
}

static inline void cleanup_close(int *fd)
{
	if (*fd >= 0)
		close(*fd);
}

/* shorthand */
#define _cleanup_free_ __attribute__((cleanup(cleanup_free)))
#define _cleanup_close_ __attribute__((cleanup(cleanup_close)))

static inline int alphanumsort(const char *a, const char *b)
{
	while (*a && *b) {
		if (isdigit((unsigned char)*a) && isdigit((unsigned char)*b)) {
			char *end_a, *end_b;
			unsigned long numa = strtoul(a, &end_a, 10);
			unsigned long numb = strtoul(b, &end_b, 10);

			if (numa < numb)
				return -1;
			if (numa > numb)
				return 1;

			a = end_a;
			b = end_b;
		} else {
			if (*a != *b)
				return (unsigned char)*a - (unsigned char)*b;
			a++;
			b++;
		}
	}

	return (unsigned char)*a - (unsigned char)*b;
}

static int entry_cmp(const void *a, const void *b, void *arg)
{
	int flags = (int)(uintptr_t) arg;

	const struct traverse_ctx_entry *ea = a, *eb = b;
	int ret;

	if (flags & TRV_SIZE) {
		if (ea->st.st_size < eb->st.st_size)
			return -1;
		if (ea->st.st_size > eb->st.st_size)
			return 1;
		/* fallback to name sort for stability */
	}

	if (flags & TRV_ALPHANUM)
		ret = alphanumsort(ea->name, eb->name);
	else
		ret = strcmp(ea->name, eb->name);

	if (flags & TRV_REVERSE)
		ret = -ret;

	return ret;
}

/* ------------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------------
 *
 * traverse_init(path, flags):
 *   - Open a traversal context starting at `path`.
 *   - Returns NULL on failure (errno set).
 *
 * traverse_next(ctx):
 *   - Returns pointer to the next entry (valid only until the next
 *     call to traverse_next() or until the directory containing it
 *     is popped).
 *   - Returns NULL when traversal is finished (errno = 0),
 *     or on error (errno preserved).
 *
 * traverse_close(ctx):
 *   - Frees all memory and closes all file descriptors.
 *
 * Flags:
 *   TRV_SORT     – sort entries alphabetically (with TRV_ALPHANUM if requested).
 *   TRV_ALPHANUM – natural alphanumeric sort instead of strcmp.
 *   TRV_REVERSE  – reverse the sort order.
 *   TRV_SIZE     – sort by file size.
 *   TRV_STAT     – always call fstatat() for entries.
 *   TRV_HIDDEN   – include hidden files (dot-prefixed). By default,
 *                  hidden files are skipped.
 */

struct traverse_ctx *traverse_init(const char *path, int flags)
{
	_cleanup_free_ struct traverse_ctx *ctx = malloc(sizeof(*ctx));
	if (!ctx)
		return NULL;

	*ctx = (struct traverse_ctx) {
		.flags = flags,
	};

	_cleanup_close_ int fd = open(path, O_RDONLY | O_DIRECTORY);
	if (fd < 0)
		return NULL;

	_cleanup_free_ struct dir_stack *d = malloc(sizeof(*d));
	if (!d)
		return NULL;
	*d = (struct dir_stack) {
		.fd = TAKE_FD(fd),
		.path = strdup(path),
	};
	if (!d->path)
		return NULL;

	ctx->stack = TAKE_PTR(d);
	return TAKE_PTR(ctx);
}

static int load_entries(struct traverse_ctx *ctx, struct dir_stack *d)
{
	char buf[64 * 1024];
	int nread;
	size_t cap = 0;

	while ((nread = syscall(SYS_getdents64, d->fd, buf, sizeof(buf))) > 0) {
		for (int bpos = 0; bpos < nread;) {
			struct linux_dirent64 *de = (struct linux_dirent64 *)(buf + bpos);
			char *name = de->d_name;
			bpos += de->d_reclen;

			if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
				continue;
			if (!(ctx->flags & TRV_HIDDEN) && name[0] == '.')
				continue;

			if (d->count == cap) {
				cap = cap ? cap * 2 : 64;
				struct traverse_ctx_entry *tmp = realloc(d->entries, cap * sizeof(*d->entries));
				if (!tmp)
					goto oom;
				d->entries = tmp;
			}

			struct traverse_ctx_entry e = { 0 };
			e.name = strdup(name);
			if (!e.name)
				goto oom;

			e.dirfd = d->fd;
			e.dtype = de->d_type;
			e.st = (struct stat) { };

			if ((ctx->flags & TRV_STAT) || de->d_type == DT_UNKNOWN) {
				if (fstatat(d->fd, name, &e.st, AT_SYMLINK_NOFOLLOW) == 0) {
					if (de->d_type == DT_UNKNOWN) {
						if (S_ISDIR(e.st.st_mode))
							e.dtype = DT_DIR;
						else if (S_ISREG(e.st.st_mode))
							e.dtype = DT_REG;
					}
				}
			}

			d->entries[d->count++] = e;
		}
	}

	if ((ctx->flags & TRV_SORT) && d->count > 1)
		qsort_r(d->entries, d->count, sizeof(*d->entries), entry_cmp, (void *)(uintptr_t) ctx->flags);

	return 0;

 oom:
	/* free any partially built list */
	for (size_t i = 0; i < d->count; i++)
		free(d->entries[i].name);
	free(d->entries);
	d->entries = NULL;
	d->count = d->index = 0;
	errno = ENOMEM;
	return -1;
}

struct traverse_ctx_entry *traverse_next(struct traverse_ctx *ctx)
{
	while (ctx->stack) {
		struct dir_stack *d = ctx->stack;

		if (!d->entries) {
			if (load_entries(ctx, d) < 0)
				return NULL;	/* errno preserved */
		}

		if (d->index < d->count) {
			struct traverse_ctx_entry *e = &d->entries[d->index++];
			if (e->dtype == DT_DIR) {
				_cleanup_close_ int childfd = openat(d->fd, e->name, O_RDONLY | O_DIRECTORY);
				if (childfd >= 0) {
					_cleanup_free_ struct dir_stack *child = malloc(sizeof(*child));
					if (!child)
						return NULL;
					*child = (struct dir_stack) {
						.fd = TAKE_FD(childfd),
						.next = ctx->stack,
					};
					ctx->stack = TAKE_PTR(child);
				}
			}
			return e;
		} else {
			ctx->stack = d->next;
			close(d->fd);
			if (d->entries) {
				for (size_t i = 0; i < d->count; i++)
					free(d->entries[i].name);
				free(d->entries);
			}
			free(d->path);
			free(d);
		}
	}

	errno = 0;		/* EOF case */
	return NULL;
}

void traverse_close(struct traverse_ctx *ctx)
{
	if (!ctx)
		return;

	while (ctx->stack) {
		struct dir_stack *d = ctx->stack;
		ctx->stack = d->next;
		close(d->fd);
		if (d->entries) {
			for (size_t i = 0; i < d->count; i++)
				free(d->entries[i].name);
			free(d->entries);
		}
		free(d->path);
		free(d);
	}
	free(ctx);
}
