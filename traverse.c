#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "traverse.h"

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s [-s] <directory>\n", argv[0]);
		return 1;
	}

	int flags = TRV_NONE;
	long count = 1;
	long long size = 0;

	if (strcmp(argv[1], "-s") == 0) {
		flags |= TRV_STAT;
		argv++;
		argc--;
	}

	struct traverse_ctx *ctx = traverse_init(argv[1], flags);

	struct traverse_ctx_entry *e;
	while ((e = traverse_next(ctx))) {
		count++;
		if (flags & TRV_STAT && e->dtype == DT_REG)
			size += e->st.st_size;
	}

	traverse_close(ctx);

	printf("Number of files: %ld\n", count);
	if (flags & TRV_STAT)
		printf("Total size: %lld bytes\n", size);

	return 0;
}
