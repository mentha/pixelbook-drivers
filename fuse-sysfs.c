#define _POSIX_C_SOURCE 200809L
#define FUSE_USE_VERSION 26

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fuse-sysfs.h"

#ifndef O_SEARCH
# define O_SEARCH 0
#endif

#ifndef NDEBUG
# define ENABLE_TRACE 1
#else
# define ENABLE_TRACE 0
#endif

static char *mnt;
static struct sysfs_entry *ent;
static size_t entcount;

static int entcomp(const void *a_, const void *b_)
{
	const struct sysfs_entry *a = a_, *b = b_;
	return strcmp(a->path, b->path);
}

static void prepare_entries(struct sysfs_entry *ent)
{
	entcount = 0;
	struct sysfs_entry *p = ent;
	while (p->path != NULL) {
		entcount++;
		p++;
	}
	qsort(ent, entcount, sizeof(*ent), entcomp);
}

static struct sysfs_entry *find_entry(const char *p)
{
	return bsearch(&p, ent, entcount, sizeof(*ent), entcomp);
}

static int sysfs_getattr(const char *path, struct stat *st)
{
	struct sysfs_entry *e = find_entry(path);
	if (e == NULL)
		return -ENOENT;
	memset(st, 0, sizeof(*st));
	st->st_mode = e->mode;
	st->st_uid = e->uid;
	st->st_gid = e->gid;
	st->st_size = e->maxsize;
	st->st_nlink = 1;
	return 0;
}

static int sysfs_readdir(const char *path, void *buf, fuse_fill_dir_t filldir)
{
	if (!strcmp(path, "."))
		path = "";
	size_t pl = strlen(path);

	struct sysfs_entry *p;
	for (p = ent; p->path != NULL; p++) {
		size_t l = strlen(p->path);
		if (l < pl)
			continue;
		const char *cp = p->path + pl;
		l -= pl;
		if (l == 0 || (pl == 0 && !strcmp(cp, "."))) {
			cp = ".";
		} else {
			if (pl != 0) {
				if (cp[0] != '/')
					continue;
				cp++;
			}
			if (strchr(cp, '/') != NULL)
				continue;
		}
		struct stat st = {
			.st_mode = p->mode,
			.st_uid = p->uid,
			.st_gid = p->gid,
			.st_size = p->maxsize,
			.st_nlink = 1
		};
		filldir(buf, cp, &st, 0);
	}
	return 0;
}

static const char *pathfix(const char *p)
{
	if (p[0] == '/')
		p++;
	if (p[0] == 0 || !strcmp(p, ".."))
		return ".";
	return p;
}

static int dfd;

static int fs_readlink(const char *path, char *buf, size_t bufsz)
{
	ssize_t r = readlinkat(dfd, pathfix(path), buf, bufsz - 1);
	if (r < 0)
		return -errno;
	buf[r] = 0;
	return 0;
}

static int trace_readlink(const char *path, char *buf, size_t bufsz)
{
	if (ENABLE_TRACE)
		printf("readlink(\"%s\", ..., %zu)", path, bufsz);
	int r = fs_readlink(path, buf, bufsz);
	if (ENABLE_TRACE) {
		if (r < 0)
			printf(" => error %d: %s\n", r, strerror(-r));
		else
			printf(" => 0, \"%s\"\n", buf);
	}
	return r;
}

static int fs_getattr(const char *path, struct stat *st)
{
	path = pathfix(path);
	if (!sysfs_getattr(path, st))
		return 0;
	int r = fstatat(dfd, path, st, AT_SYMLINK_NOFOLLOW);
	if (r)
		return -errno;
	return 0;
}

static int trace_getattr(const char *path, struct stat *st)
{
	if (ENABLE_TRACE)
		printf("getattr(\"%s\")", path);
	int r = fs_getattr(path, st);
	if (ENABLE_TRACE) {
		if (r < 0)
			printf(" => error %d: %s\n", r, strerror(-r));
		else
			printf(" => 0\n");
	}
	return r;
}

static int fs_truncate(const char *path, off_t sz)
{
	path = pathfix(path);
	if (find_entry(path))
		return 0;
	int fd = openat(dfd, path, O_RDONLY | O_NOFOLLOW | O_NOCTTY);
	if (fd < 0)
		return -errno;
	int e = 0;
	if (ftruncate(fd, sz))
		e = errno;
	close(fd);
	if (e)
		return -e;
	return 0;
}

static int trace_truncate(const char *path, off_t sz)
{
	if (ENABLE_TRACE)
		printf("truncate(\"%s\", %zu)", path, sz);
	int r = fs_truncate(path, sz);
	if (ENABLE_TRACE) {
		if (r < 0)
			printf(" => error %d: %s\n", r, strerror(-r));
		else
			printf(" => 0\n");
	}
	return r;
}

#define FH_INVALID ((uintptr_t) 0 - 1)

static int fs_opendir(const char *path, struct fuse_file_info *fi)
{
	static_assert(sizeof(uintptr_t) <= sizeof(fi->fh), "fi->fh too small");
	path = pathfix(path);
	int fd = openat(dfd, path, O_RDONLY | O_SEARCH | O_DIRECTORY |
			O_NOFOLLOW | O_NOCTTY);
	if (fd < 0) {
		int e = errno;
		if (find_entry(path)) {
			fi->fh = FH_INVALID;
			return 0;
		}
		return -e;
	}
	DIR *d = fdopendir(fd);
	if (d == NULL) {
		int e = errno;
		close(fd);
		return -e;
	}
	fi->fh = (uintptr_t) d;
	return 0;
}

static int trace_opendir(const char *path, struct fuse_file_info *fi)
{
	if (ENABLE_TRACE)
		printf("opendir(\"%s\")", path);
	int r = fs_opendir(path, fi);
	if (ENABLE_TRACE) {
		if (r < 0)
			printf(" => error %d: %s\n", r, strerror(-r));
		else
			printf(" => 0, %p\n", (void *) fi->fh);
	}
	return r;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filldir,
		off_t ignored_, struct fuse_file_info *fi)
{
	path = pathfix(path);
	if (fi->fh == FH_INVALID)
		return sysfs_readdir(path, buf, filldir);
	DIR *d = (DIR *) fi->fh;
	size_t pl = strlen(path);
	struct dirent *e;
	for (e = readdir(d); e != NULL; e = readdir(d)) {
		size_t el = strlen(e->d_name);
		char np[el + pl + 2];
		strcpy(np, path);
		strcat(np, "/");
		strcpy(np, e->d_name);
		if (!find_entry(np))
			filldir(buf, e->d_name, NULL, 0);
	}
	sysfs_readdir(path, buf, filldir);
	return 0;
}

static int trace_readdir(const char *path, void *buf, fuse_fill_dir_t filldir,
		off_t ignored_, struct fuse_file_info *fi)
{
	if (ENABLE_TRACE)
		printf("readdir(\"%s\", %p)", path, (void *) fi->fh);
	int r = fs_readdir(path, buf, filldir, ignored_, fi);
	if (ENABLE_TRACE) {
		if (r < 0)
			printf(" => error %d: %s\n", r, strerror(-r));
		else
			printf(" => 0\n");
	}
	return r;
}

static int fs_releasedir(const char *path, struct fuse_file_info *fi)
{
	if (fi->fh == FH_INVALID)
		return 0;
	DIR *d = (DIR *) fi->fh;
	if (closedir(d))
		return -errno;
	return 0;
}

static int trace_releasedir(const char *path, struct fuse_file_info *fi)
{
	if (ENABLE_TRACE)
		printf("readdir(\"%s\", %p)", path, (void *) fi->fh);
	int r = fs_releasedir(path, fi);
	if (ENABLE_TRACE) {
		if (r < 0)
			printf(" => error %d: %s\n", r, strerror(-r));
		else
			printf(" => 0\n");
	}
	return r;
}

struct fdesc {
	int fd;
	struct sysfs_entry *e;
	void *buf;
	size_t sz;
	bool write_pending;
};

#define FDESC(x) ((struct fdesc *) (x))

static int fs_open(const char *path, struct fuse_file_info *fi)
{
	struct fdesc *d = malloc(sizeof(*d));
	if (d == NULL)
		return -ENOBUFS;
	memset(d, 0, sizeof(*d));
	path = pathfix(path);
	struct sysfs_entry *e = find_entry(path);
	if (e) {
		d->fd = -1;
		d->e = e;
		fi->fh = (uintptr_t) d;
		return 0;
	}
	int fd = openat(dfd, path, (fi->flags | O_NOFOLLOW | O_NOCTTY) &
			~(O_CREAT | O_NONBLOCK));
	if (fd < 0) {
		free(d);
		return -errno;
	}
	d->fd = fd;
	fi->fh = (uintptr_t) d;
	return 0;
}

static int trace_open(const char *path, struct fuse_file_info *fi)
{
	if (ENABLE_TRACE)
		printf("open(\"%s\", 0x%x)", path, fi->flags);
	int r = fs_open(path, fi);
	if (ENABLE_TRACE) {
		if (r < 0)
			printf(" => error %d: %s\n", r, strerror(-r));
		else
			printf(" => %d\n", FDESC(fi->fh)->fd);
	}
	return r;
}

static int fs_read(const char *path, char *buf, size_t sz, off_t off,
		struct fuse_file_info *fi)
{
	struct fdesc *d = FDESC(fi->fh);
	if (d->fd >= 0) {
		ssize_t r = pread(d->fd, buf, sz, off);
		if (r < 0)
			return -errno;
		return r;
	}

	if (d->buf == NULL) {
		if (d->e->read == NULL)
			return -ENOSYS;
		int r = d->e->read(d->e, &d->buf, &d->sz);
		if (r < 0)
			return r;
	}
	if (off >= d->sz)
		return 0;
	size_t msz = d->sz - off;
	size_t rsz = (msz > sz) ? sz : msz;
	memcpy(buf, ((char *) d->buf) + off, rsz);
	return rsz;
}

static int trace_read(const char *path, char *buf, size_t sz, off_t off,
		struct fuse_file_info *fi)
{
	if (ENABLE_TRACE)
		printf("pread(%d \"%s\", ..., %zu, %zu)", FDESC(fi->fh)->fd, path, sz, off);
	int r = fs_read(path, buf, sz, off, fi);
	if (ENABLE_TRACE) {
		if (r < 0)
			printf(" => error %d: %s\n", r, strerror(-r));
		else
			printf(" => %d\n", r);
	}
	return r;
}

static int bufalloc(void **buf, size_t *bufsz, size_t sz, off_t off)
{
	if (*buf == NULL) {
		*buf = malloc(sz + off);
		if (*buf == NULL)
			return 1;
		if (off > 0)
			memset(buf, 0, off);
		*bufsz = sz + off;
		return 0;
	}

	if (sz + off <= *bufsz)
		return 0;

	char *nbuf = realloc(*buf, sz + off);
	if (nbuf == NULL)
		return 1;
	if (off > *bufsz)
		memset(nbuf + *bufsz, 0, off - *bufsz);
	*buf = nbuf;
	*bufsz = sz + off;
	return 0;
}

static int fs_write(const char *path, const char *buf, size_t sz, off_t off,
		struct fuse_file_info *fi)
{
	struct fdesc *d = FDESC(fi->fh);
	if (d->fd >= 0) {
		ssize_t r = pwrite(d->fd, buf, sz, off);
		if (r < 0)
			return -errno;
		return r;
	}

	if (!d->e->write_merge) {
		if (d->e->write == NULL)
			return -ENOSYS;
		int r = d->e->write(d->e, buf, sz);
		if (r < 0)
			return -errno;
		return r;
	}

	if (sz == 0)
		return 0;
	if (bufalloc(&d->buf, &d->sz, sz, off))
		return -errno;
	memcpy(((char *) d->buf) + off, buf, sz);
	d->write_pending = true;
	return sz;
}

static int trace_write(const char *path, const char *buf, size_t sz, off_t off,
		struct fuse_file_info *fi)
{
	if (ENABLE_TRACE)
		printf("pwrite(%d \"%s\", ..., %zu, %zu)",
				FDESC(fi->fh)->fd, path, sz, off);
	int r = fs_write(path, buf, sz, off, fi);
	if (ENABLE_TRACE) {
		if (r < 0)
			printf(" => error %d: %s\n", r, strerror(-r));
		else
			printf(" => %d\n", r);
	}
	return r;
}

static int fs_flush(const char *path, struct fuse_file_info *fi)
{
	struct fdesc *d = (void *) fi->fh;
	if (d->fd < 0 && d->write_pending) {
		if (d->e->write == NULL)
			return -ENOSYS;
		int r = d->e->write(d->e, d->buf, d->sz);
		if (r < 0)
			return -errno;
	}
	return 0;
}

static int trace_flush(const char *path, struct fuse_file_info *fi)
{
	if (ENABLE_TRACE)
		printf("flush(%d \"%s\")", FDESC(fi->fh)->fd, path);
	int r = fs_flush(path, fi);
	if (ENABLE_TRACE) {
		if (r < 0)
			printf(" => error %d: %s\n", r, strerror(-r));
		else
			printf(" => %d\n", r);
	}
	return r;
}

static int fs_release(const char *path, struct fuse_file_info *fi)
{
	path = pathfix(path);
	struct fdesc *d = (void *) fi->fh;
	if (d->fd >= 0) {
		if (close(d->fd))
			return -errno;
	} else {
		if (d->buf)
			free(d->buf);
	}
	free(d);
	return 0;
}

static int trace_release(const char *path, struct fuse_file_info *fi)
{
	if (ENABLE_TRACE)
		printf("close(%d \"%s\")", FDESC(fi->fh)->fd, path);
	int r = fs_release(path, fi);
	if (ENABLE_TRACE) {
		if (r < 0)
			printf(" => error %d: %s\n", r, strerror(-r));
		else
			printf(" => %d\n", r);
	}
	return r;
}

struct fuse_operations fsops = {
	.readlink = trace_readlink,
	.getattr = trace_getattr,
	.truncate = trace_truncate,

	.opendir = trace_opendir,
	.readdir = trace_readdir,
	.releasedir = trace_releasedir,

	.open = trace_open,
	.read = trace_read,
	.write = trace_write,
	.flush = trace_flush,
	.release = trace_release,
};

int main(int argc, char **argv)
{
	int r = fuse_sysfs_init(argc, argv, &mnt, &ent);
	if (r)
		return r;

	dfd = open(mnt, O_RDONLY | O_SEARCH | O_DIRECTORY |
			O_NOFOLLOW | O_NOCTTY);
	if (dfd < 0) {
		perror("cannot open original directory");
		r = 1;
		goto quit;
	}

	prepare_entries(ent);

	char argv1[] = "-o";
	char argv2[] = "allow_other,default_permissions,auto_unmount,nonempty,"
		"nodev,noexec,nosuid";
	char argv3[] = "-f";
	char argv4[] = "-s";
	char *fuseargv[] = {
		argv[0],
		argv1,
#ifndef NDEBUG
		(geteuid() != 0) ? (strchr(argv2, ',') + 1) :
#endif
			argv2,
		argv3,
		argv4,
		mnt
	};
	r = fuse_main(sizeof(fuseargv) / sizeof(fuseargv[0]), fuseargv,
			&fsops, NULL);

	close(dfd);
quit:
	fuse_sysfs_fini(mnt, ent);
	return r;
}

int parse_num(const char *s, size_t sz, long long *res)
{
	while (*s != 0 && sz > 0 && isspace(*s)) {
		s++;
		sz--;
	}
	*res = 0;
	int sign = 1;
	int base = 10;
	int valid = 0;
	if (*s == '-') {
		s++;
		sz--;
		sign = -1;
	}
	if (*s == '0') {
		s++;
		sz--;
		if (sz == 0 || *s == 0)
			return 0;
		if (*s == 'x' || *s == 'X') {
			s++;
			sz--;
			base = 16;
		} else if (*s >= '0' && *s <= '7') {
			base = 8;
		} else {
			errno = EINVAL;
			return 1;
		}
	}
	while (*s != 0 && sz > 0 && isalnum(*s)) {
		*res *= base;
		if (*s >= '0' && (*s <= '7' || (base >= 10 && *s <= '9'))) {
			*res += *s - '0';
		} else if (base >= 16) {
			if (*s >= 'A' && *s <= 'F') {
				*res += *s - 'A' + 10;
			} else if (*s >= 'a' && *s <= 'f') {
				*res += *s - 'a' + 10;
			} else {
				errno = EINVAL;
				return 1;
			}
		} else {
			errno = EINVAL;
			return 1;
		}
		valid = 1;
		s++;
		sz--;
	}
	while (*s != 0 && sz > 0 && isspace(*s)) {
		s++;
		sz--;
	}
	if (valid && (*s == 0 || sz == 0)) {
		*res *= sign;
		return 0;
	}
	errno = EINVAL;
	return 1;
}
