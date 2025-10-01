#ifndef TRAVERSE_H
#define TRAVERSE_H

/*
 * traverse.h - Directory traversal library
 *
 * API contract:
 *
 *  - traverse_init(path, flags):
 *      Initialize a traversal context for the given directory.
 *      Opens the directory and prepares an internal stack of directories
 *      to visit. Returns NULL on error.
 *
 *  - traverse_next(ctx):
 *      Returns the next directory entry in depth-first order, or NULL
 *      if traversal is finished or an error occurs.
 *
 *      IMPORTANT LIFETIME RULE:
 *          The returned pointer refers directly to the internal array of
 *          entries belonging to the directory currently being traversed.
 *          It remains valid only until:
 *              * the next call to traverse_next(), OR
 *              * the directory containing that entry is popped from the stack.
 *
 *          Callers must copy out any fields they want to keep.
 *
 *  - traverse_close(ctx):
 *      Frees all memory and closes all open file descriptors associated
 *      with the traversal context. Safe to call with NULL.
 *
 * Flags:
 *      TRV_NONE      - no special behavior
 *      TRV_HIDDEN    - show hidden files (names starting with '.')
 *      TRV_STAT      - perform fstatat() on each entry
 *      TRV_SORT      - sort entries alphabetically before returning them
 *      TRV_REVERSE   - reverse the sort order
 *      TRV_ALPHANUM   - use alphanumeric sort (e.g. A9 < A10)
 *      TRV_SIZE      - sort by size (implies TRV_STAT)
 */

#include <sys/types.h>
#include <sys/stat.h>

#define BIT(x) ((1) << (x))

#define TRV_NONE	0
#define TRV_HIDDEN	BIT(0)
#define TRV_STAT	BIT(1)
#define TRV_SORT	BIT(2)
#define TRV_REVERSE	BIT(3)
#define TRV_ALPHANUM	BIT(4)
#define TRV_SIZE	BIT(5)

// Opaque handle
struct traverse_ctx;

// Entry structure returned by traverse_next()
struct traverse_ctx_entry {
	char *name;		// entry name
	int dirfd;		// parent directory fd
	unsigned char dtype;	// DT_* type
	struct stat st;		// valid if TRV_STAT
};

// Functions
struct traverse_ctx* traverse_init(const char *path, int flags);
struct traverse_ctx_entry* traverse_next(struct traverse_ctx *ctx);
void traverse_close(struct traverse_ctx *ctx);

#endif
