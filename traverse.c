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

static int calculate_size;
static long long file_count = 1;
static long long size;

void traverse(int dirfd)
{
	char buf[64 * 1024];

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
			int stated = 0;

			bpos += d->d_reclen;
			if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
				continue;

			file_count++;

			struct stat st;
			if (d->d_type == DT_UNKNOWN) {
				int r = fstatat(dirfd, name, &st, AT_SYMLINK_NOFOLLOW);
				if (r != -1) {
					stated = 1;
					if (S_ISREG(st.st_mode))
						d->d_type = DT_REG;
					else if (S_ISDIR(st.st_mode))
						d->d_type = DT_DIR;
				}
			}
			if (calculate_size && d->d_type == DT_REG) {
				if (!stated) {
					int r = fstatat(dirfd, name, &st, AT_SYMLINK_NOFOLLOW);
					if (r == -1) {
						perror("fstatat");
						continue;
					}
				}
				size += st.st_size;
			}

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
	}
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

	int fd = open(argv[1], O_RDONLY | O_DIRECTORY);
	if (fd == -1) {
		perror("open");
		return 1;
	}

	traverse(fd);
	close(fd);

	printf("Total files: %lld\n", file_count);
	if (calculate_size)
		printf("Total size: %lld bytes\n", size);
	return 0;
}
