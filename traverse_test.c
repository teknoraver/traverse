#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "traverse.h"

static int trv(const char *dir, int flags)
{
	struct traverse_ctx *ctx = traverse_init(dir, flags);

	struct traverse_ctx_entry *e;
	while ((e = traverse_next(ctx)))
		puts(e->name);

	traverse_close(ctx);

	return 0;
}

#define TEST(x) { puts("-----[" #x "]-----") ; trv(argv[1], x); }

int main(int argc, char *argv[])
{
	if (argc < 2)
		return 1;

	TEST(TRV_NONE);
	TEST(TRV_HIDDEN);
	TEST(TRV_SORT);
	TEST(TRV_SORT | TRV_REVERSE);
	TEST(TRV_SORT | TRV_ALPHANUM);
	TEST(TRV_SORT | TRV_STAT | TRV_SIZE);

	return 0;
}
