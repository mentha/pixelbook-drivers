#ifndef fuse_sysfs_h_
#define fuse_sysfs_h_

#include <stdbool.h>
#include <sys/stat.h>
#include <stddef.h>

struct sysfs_entry;
typedef int (*sysfs_entry_read_fn)(struct sysfs_entry *ent,
		void **result, size_t *resultsz);
typedef int (*sysfs_entry_write_fn)(struct sysfs_entry *ent,
		const void *buffer, size_t bufsz);

struct sysfs_entry {
	/** Path relative to root */
	const char *path;
	mode_t mode;
	size_t maxsize;
	sysfs_entry_read_fn read;
	sysfs_entry_write_fn write;
	uid_t uid;
	gid_t gid;
	bool write_merge;
};

/**
 * Initialize filesystem.
 * \param[out] mnt path to mount point.
 * \param[out] entries NULL-terminated list of special entries.
 * \returns 0 on successful completion.
 */
extern int fuse_sysfs_init(int argc, char **argv,
		char **mnt, struct sysfs_entry **entries);

extern void fuse_sysfs_fini(char *mnt, struct sysfs_entry *entries);

/* misc */

extern int parse_num(const char *num, size_t numsz, long long *res);

#endif
