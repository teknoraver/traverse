#define _XOPEN_SOURCE 700
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fts.h>

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <directory> [directory...]\n", argv[0]);
		return EXIT_FAILURE;
	}
	char **paths = &argv[1];

	FTS *ftsp;
	FTSENT *ent;
	long count = 0;

	ftsp = fts_open(paths, FTS_PHYSICAL | FTS_NOSTAT, NULL);
	if (!ftsp) {
		perror("fts_open");
		return EXIT_FAILURE;
	}

	while ((ent = fts_read(ftsp)) != NULL) {
		switch (ent->fts_info) {
		case FTS_F:	// regular file
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
	return 0;
}
