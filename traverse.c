#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <dirent.h>
#include <sys/syscall.h>

// Linux kernel dirent format
struct linux_dirent64 {
	ino64_t d_ino;
	off64_t d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[];
};

static long long file_count = 1;

void traverse(int dirfd)
{
	char buf[4096];

	for (;;) {
		int nread = syscall(SYS_getdents64, dirfd, buf, sizeof(buf));
		if (nread == -1) {
			perror("getdents64");
			return;
		}
		if (nread == 0)
			break;

		for (int bpos = 0; bpos < nread;) {
			struct linux_dirent64 *d = (struct linux_dirent64 *)(buf + bpos);
			char *name = d->d_name;

			if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
				file_count++;

				if (d->d_type == DT_DIR) {
					int childfd = openat(dirfd, name, O_RDONLY | O_DIRECTORY);
					if (childfd == -1) {
						perror("openat");
					} else {
						traverse(childfd);
						close(childfd);
					}
				}
			}
			bpos += d->d_reclen;
		}
	}
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
		return EXIT_FAILURE;
	}

	int fd = open(argv[1], O_RDONLY | O_DIRECTORY);
	if (fd == -1) {
		perror("open");
		return EXIT_FAILURE;
	}

	traverse(fd);
	close(fd);

	printf("Total files: %lld\n", file_count);
	return EXIT_SUCCESS;
}
