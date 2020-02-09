#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int bl_open(void)
{
	DIR *d = opendir("/sys/class/drm_dp_aux_dev");
	if (d == NULL)
		return -1;

	int ret = -1;
	int err = 0;

	regex_t re;
	if (regcomp(&re, "/card[0-9]+-eDP-[0-9]+/", REG_EXTENDED)) {
		err = errno;
		goto exit_close_d;
	}

	const char *devname = NULL;
	struct dirent *e;
	for (e = readdir(d); e != NULL; e = readdir(d)) {
		if (strlen(e->d_name) < 10 || memcmp(e->d_name, "drm_dp_aux", 10))
			continue;
		const size_t lbufsz = 1000;
		char buf[lbufsz];
		if (readlinkat(dirfd(d), e->d_name, buf, lbufsz) < 0)
			continue;
		if (regexec(&re, buf, 0, NULL, 0) == 0) {
			devname = e->d_name;
			break;
		}
	}

	regfree(&re);

	if (devname != NULL) {
		char path[100];
		snprintf(path, 100, "/dev/%s", devname);
		ret = open(path, O_RDWR | O_NOCTTY);
		if (ret < 0)
			err = errno;
	}

exit_close_d:
	closedir(d);
	if (ret < 0)
		errno = err;
	return ret;
}

void bl_close(int fd)
{
	close(fd);
}

int bl_setup(int fd)
{
	uint8_t b;
	/* 0x721 MODE_SET_REGISTER */
	if (pread(fd, &b, 1, 0x721) != 1)
		return -1;
	b &= 0xf8;
	b |= 0x2;
	if (pwrite(fd, &b, 1, 0x721) != 1)
		return -1;
	/* 0x724 PWMGEN_BIT_COUNT */
	b = 0x10; /* magic number that works */
	if (pwrite(fd, &b, 1, 0x724) != 1)
		return -1;
	/* 0x728 BACKLIGHT_FREQ_SET */
	b = 0x01;
	if (pwrite(fd, &b, 1, 0x728) != 1)
		return -1;
	return 0;
}

int bl_max(int fd, int *value)
{
	*value = 0xffff;
	return 0;
}

int bl_set(int fd, int value)
{
	if (value < 0)
		value = 0;
	if (value > 0xffff)
		value = 0xffff;
	uint8_t b[2];
	b[0] = value >> 8;
	b[1] = value & 0xff;
	/* 0x722 BRIGHTNESS_MSB */
	if (pwrite(fd, b, 2, 0x722) != 2)
		return -1;
	return 0;
}

int bl_get(int fd, int *value)
{
	uint8_t buf[2];
	if (pread(fd, buf, 2, 0x722) != 2)
		return -1;
	*value = (buf[0] << 8) | buf[1];
	return 0;
}
