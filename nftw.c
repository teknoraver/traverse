#define _XOPEN_SOURCE 700
#include <sys/types.h>
#include <sys/stat.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int calculate_size;
static long count;
static long long size;

static int count_size(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
	count++;
	if (calculate_size && typeflag == FTW_F)
		size += sb->st_size;
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s [-s] <directory>\n", argv[0]);
		return 1;
	}

	if (strcmp(argv[1], "-s") == 0) {
		calculate_size = 1;
		argv++;
		argc--;
	}

	if (nftw(argv[1], count_size, 16, FTW_PHYS) == -1) {
		perror("nftw");
		return 1;
	}

	printf("Number of files: %ld\n", count);
	if (calculate_size)
		printf("Total size: %lld bytes\n", size);
	return 0;
}
