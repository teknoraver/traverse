#define _XOPEN_SOURCE 700
#include <sys/types.h>
#include <sys/stat.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>

static long count;

static int count_size(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
	count++;
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
		return 1;
	}

	if (nftw(argv[1], count_size, 16, FTW_PHYS) == -1) {
		perror("nftw");
		return 1;
	}

	printf("Number of files: %ld\n", count);
	return 0;
}
