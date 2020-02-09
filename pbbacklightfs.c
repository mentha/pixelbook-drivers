#include "fuse-sysfs.h"
#include "edp-backlight.h"
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define SPEED 1000 /* milliseconds taken to change from 0 to max */
#define MIN_INTERVAL 10
#define MIN_PERIOD 300

static int brimap(long long b)
{
	long long maxb = 0xffff;
	long long lowestb = maxb / 100;
	long long minb = 300; /* minimum useful brightness */
	if (b == 0)
		return 0;
	if (b < lowestb)
		return minb;
	long long tb = b - lowestb;
	long long maxtb = maxb - lowestb;
	return b * b * (maxb - lowestb) / (maxtb * maxtb) + minb;
}

static void makestep(int maxbri, int bri, int realbri,
		int *step, int *intval)
{
	int total = bri - realbri;
	if (total == 0) {
		*step = 0;
		*intval = -1;
		return;
	}

	int atotal = abs(total);
	int period = (atotal * SPEED + maxbri / 2) / maxbri;
	if (period < MIN_PERIOD)
		period = MIN_PERIOD;

	int max_step_count = period / MIN_INTERVAL;

	*step = total / max_step_count;
	if (*step == 0)
		*step = total < 0 ? -1 : 1;
	*intval = MIN_INTERVAL;
}

static void child(int sock)
{
	int brifd = bl_open();
	if (brifd < 0)
		goto exit;

	if (bl_setup(brifd))
		goto exit_close_brifd;

	char sndbuf = 0;
	if (send(sock, &sndbuf, sizeof(sndbuf), 0) != sizeof(sndbuf))
		goto exit_close_brifd;

	int maxbri;
	if (bl_max(brifd, &maxbri))
		goto exit_close_brifd;

	int bri;
	if (bl_get(brifd, &bri))
		bri = 30000;
	int realbri = bri;

	int step, intval;
	makestep(maxbri, bri, realbri, &step, &intval);

	struct pollfd pfd = {
		.fd = sock,
		.events = POLLIN
	};

	int retry = 10000;

	for (;;) {
		int pr = poll(&pfd, 1, intval);
		if (pr < 0) {
			if (errno == EAGAIN && retry > 0) {
				retry--;
				continue;
			}
			perror("failed poll");
			break;
		}

		if (pr > 0) {
			int newbri;
			ssize_t rs = recv(sock, &newbri, sizeof(newbri), 0);
			if (rs == sizeof(newbri)) {
				if (newbri < 0)
					newbri = 0;
				if (newbri > maxbri)
					newbri = maxbri;
				bri = newbri;
			}
			if (rs == 0 || (rs < 0 && errno != EAGAIN))
				break;
		}

		makestep(maxbri, bri, realbri, &step, &intval);

		realbri += step;
		if (bl_set(brifd, brimap(realbri))) {
			perror("cannot update brightness");
			break;
		}
	}

exit_close_brifd:
	bl_close(brifd);
exit:
	close(sock);
	exit(0);
}

static int child_setup(void)
{
	int sock[2];
	if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sock)) {
		perror("cannot create sock");
		return -1;
	}
	pid_t p = fork();
	if (p < 0) {
		perror("cannot fork");
		close(sock[0]);
		close(sock[1]);
		return -1;
	}
	if (p > 0) {
		close(sock[0]);
		char buf;
		if (recv(sock[1], &buf, sizeof(buf), 0) <= 0) {
			close(sock[1]);
			return -1;
		}
		return sock[1];
	}
	close(sock[1]);
	child(sock[0]);
	exit(0);
	return 0;
}

#ifndef NDEBUG
# define ENABLE_TRACE 1
#else
# define ENABLE_TRACE 0
#endif

static int request_bri_update(int sock, int bri)
{
	return send(sock, &bri, sizeof(bri), 0) <= 0;
}

static int sock;
static int bfd;

static int lastbri;

static int bri_read(struct sysfs_entry *ent,
		void **result, size_t *resultsz)
{
	char *buf = malloc(20);
	if (buf == NULL)
		return -1;
	sprintf(buf, "%d\n", lastbri);
	*result = buf;
	*resultsz = strlen(buf);
	return *resultsz;
}

static int bri_write(struct sysfs_entry *ent,
		const void *buffer, size_t bufsz)
{
	long long n;
	if (parse_num(buffer, bufsz, &n))
		return -1;
	if (n < 0)
		n = 0;
	int max;
	if (!bl_max(bfd, &max))
		if (n > max)
			n = max;
	lastbri = n;
	if (request_bri_update(sock, lastbri))
		return -1;
	return bufsz;
}

static int bri_read_direct(struct sysfs_entry *ent,
		void **result, size_t *resultsz)
{
	char *buf = malloc(20);
	if (buf == NULL)
		return -1;
	int n;
	if (bl_get(bfd, &n))
		n = lastbri;
	sprintf(buf, "%d\n", n);
	*result = buf;
	*resultsz = strlen(buf);
	return *resultsz;
}

static int bri_read_max(struct sysfs_entry *ent,
		void **result, size_t *resultsz)
{
	char *buf = malloc(20);
	if (buf == NULL)
		return -1;
	int n;
	if (bl_max(bfd, &n))
		n = 0xffff;
	sprintf(buf, "%d\n", n);
	*result = buf;
	*resultsz = strlen(buf);
	return *resultsz;
}

struct sysfs_entry entries[] = {
	{
		"brightness",
		S_IFREG | 0644,
		4096,
		bri_read,
		bri_write,
	},
	{
		"actual_brightness",
		S_IFREG | 0444,
		4096,
		bri_read_direct
	},
	{
		"max_brightness",
		S_IFREG | 0444,
		4096,
		bri_read_max
	},
	{ NULL }
};

int fuse_sysfs_init(int argc, char **argv,
		char **mnt, struct sysfs_entry **ent)
{
	if (argc != 2 || argv[1][0] == '-') {
		fprintf(stderr, "usage: %s <dir>\n",
				argc == 1 ? argv[0] : "pbbacklightfs");
		return 1;
	}
	sock = child_setup();
	bfd = bl_open();
	if (bfd < 0) {
		perror("cannot open edp aux");
		close(sock);
		return 1;
	}
	if (bl_get(bfd, &lastbri))
		lastbri = 30000;
	*mnt = argv[1];
	*ent = entries;
	return 0;
}

void fuse_sysfs_fini(char *mnt, struct sysfs_entry *ent)
{
	close(sock);
	bl_close(bfd);
}
