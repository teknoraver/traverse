#define _XOPEN_SOURCE 700
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fts.h>

int main(int argc, char *argv[])
{
	int calculate_size = 0;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s [-s] <directory>\n", argv[0]);
		return 1;
	}

	if (strcmp(argv[1], "-s") == 0) {
		calculate_size = 1;
		argv++;
		argc--;
	}

	char **paths = &argv[1];

	FTS *ftsp;
	FTSENT *ent;
	long count = 0;
	long long size = 0;

	ftsp = fts_open(paths, FTS_PHYSICAL | (calculate_size ? 0 : FTS_NOSTAT), NULL);
	if (!ftsp) {
		perror("fts_open");
		return 1;
	}

	while ((ent = fts_read(ftsp)) != NULL) {
		switch (ent->fts_info) {
		case FTS_F:	// regular file
			count++;
			if (calculate_size)
				size += ent->fts_statp->st_size;
			break;
		case FTS_D:	// directory
		case FTS_SL:	// symbolic link
		case FTS_SLNONE:	// broken symlink
		case FTS_DEFAULT:	// other (fifo, socket, device, etc.)
		case FTS_NS:	// stat() failed
		case FTS_NSOK:	// no stat() requested
			count++;
			break;
		default:
			break;

		}
	}

	if (errno != 0) {
		perror("fts_read");
		fts_close(ftsp);
		return 1;
	}

	if (fts_close(ftsp) < 0) {
		perror("fts_close");
		return 1;
	}

	printf("Number of files: %ld\n", count);
	if (calculate_size)
		printf("Total size: %lld bytes\n", size);

	return 0;
}
