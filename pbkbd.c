#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libevdev/libevdev-uinput.h>
#include <libevdev/libevdev.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define INPUTPATH "/dev/input"
#define FN_LEAK_UNMAPPED

static void print_help(const char *progname)
{
	printf("usage: %s [-vq]\n"
			"Pixelbook keyboard driver.\n"
			"Options:\n"
			"  -v  increase verbosity\n"
			"  -q  decrease verbosity\n"
			, progname);
}

#define errstr strerror(errno)

enum {
	FATAL,
	ERROR,
	WARN,
	INFO,
	DEBUG, /* show less debug info */
	DEBUG2, /* show more debug info */
	DEBUG3, /* log all keystrokes */
	DEBUG4, /* log all keyboard events */
};
static int verbosity = INFO;
#define LOGENABLED(lvl) (verbosity >= (lvl))
#define LOG(lvl, ...) do { \
	if (LOGENABLED(lvl)) \
	printf(#lvl ": " __VA_ARGS__); \
} while (0)

static int stop = 0;

static void sighandler(int sig)
{
	stop = 1;
}

static int sigsetup(void)
{
	struct sigaction sa = {
		.sa_handler = sighandler
	};
	if (sigaction(SIGHUP, &sa, NULL) ||
			sigaction(SIGINT, &sa, NULL) ||
			sigaction(SIGTERM, &sa, NULL))
		return 1;
	return 0;
}

static int scan_single_input(int dfd, const char *devnodename)
{
	int ret = -1;
	LOG(DEBUG2, "found device " INPUTPATH "/%s\n", devnodename);
	int f = openat(dfd, devnodename, O_RDWR | O_NOCTTY);
	if (f < 0) {
		LOG(DEBUG2, "device cannot be opened: %s\n", errstr);
		goto exit;
	}
	struct libevdev *dev;
	if (libevdev_new_from_fd(f, &dev)) {
		LOG(DEBUG2, "libevdev rejects device: %s\n", errstr);
		goto exit_f;
	}

	if (LOGENABLED(DEBUG2)) {
		LOG(DEBUG2, "device info:\n"
				"\tname: '%s'\n"
				"\tphys: '%s'\n"
				"\tuniq: '%s'\n"
				"\tid_product: '0x%x'\n"
				"\tid_vendor: '0x%x'\n"
				"\tid_bustype: '0x%x'\n"
				"\tid_version: '0x%x'\n",
				libevdev_get_name(dev),
				libevdev_get_phys(dev),
				libevdev_get_uniq(dev),
				libevdev_get_id_product(dev),
				libevdev_get_id_vendor(dev),
				libevdev_get_id_bustype(dev),
				libevdev_get_id_version(dev));
	}

	if (libevdev_get_id_bustype(dev) == BUS_I8042) {
		ret = f;
		LOG(INFO, "found keyboard %s\n", devnodename);
	}

	libevdev_free(dev);
exit_f:
	if (ret < 0)
		close(f);
exit:
	return ret;
}

static int scan_pbkbd(void)
{
	LOG(DEBUG2, "scanning " INPUTPATH "\n");
	DIR *d = opendir(INPUTPATH);
	if (d == NULL) {
		LOG(ERROR, "cannot scan devices: %s\n", errstr);
		goto exit;
	}
	int dfd = dirfd(d);

	int ret = -1;
	struct dirent *e;
	for (e = readdir(d); e != NULL; e = readdir(d)) {
		if (!(strlen(e->d_name) > 5 &&
					!memcmp(e->d_name, "event", 5)))
			continue;
		ret = scan_single_input(dfd, e->d_name);
		if (ret >= 0)
			break;
	}
	closedir(d);

exit:
	return ret;
}

#include "keymap.h"

#define keymap_direct_size (sizeof(keymap_direct) / sizeof(int))
#define keymap_fn_size (sizeof(keymap_fn) / sizeof(int))
#define keymap_combo_size (sizeof(keymap_combo) / sizeof(int *))

static struct libevdev_uinput *open_uinputdev(int uinputfd, const char *name)
{
	LOG(DEBUG2, "creating uinput device\n");
	struct libevdev *refdev = libevdev_new();
	libevdev_set_name(refdev, name);

	libevdev_enable_event_type(refdev, EV_SYN);
	libevdev_enable_event_code(refdev, EV_SYN, SYN_REPORT, NULL);

	libevdev_enable_event_type(refdev, EV_MSC);
	libevdev_enable_event_code(refdev, EV_MSC, MSC_SCAN, NULL);

	libevdev_enable_event_type(refdev, EV_KEY);
	int i;
	for (i = 0; i < keymap_direct_size; i++)
		if (keymap_direct[i] > 0)
			libevdev_enable_event_code(refdev, EV_KEY, keymap_direct[i], NULL);
	for (i = 0; i < keymap_fn_size; i++)
		if (keymap_fn[i] > 0)
			libevdev_enable_event_code(refdev, EV_KEY, keymap_fn[i], NULL);
	for (i = 0; i < keymap_combo_size; i++) {
		const int *p;
		for (p = keymap_combo[i]; *p > 0; p++)
			libevdev_enable_event_code(refdev, EV_KEY, *p, NULL);
	}

	struct libevdev_uinput *r;
	if (libevdev_uinput_create_from_device(refdev,
				LIBEVDEV_UINPUT_OPEN_MANAGED, &r))
		r = NULL;

	libevdev_free(refdev);
	return r;
}

static int uinput_write_event(struct libevdev_uinput *uinput,
		unsigned int type, unsigned int code, int value)
{
	LOG(DEBUG4, "writing uinput type 0x%x code 0x%x value 0x%x\n",
			type, code, value);
	return libevdev_uinput_write_event(uinput, type, code, value);
}

static int event_emit(struct libevdev_uinput *uinput, int scan, int key, int release)
{
	int ret = 0;
	if (release)
		release = 1;
	LOG(DEBUG3, "emit scan 0x%x key 0x%x %s\n", scan, key, release ? "RELEASE" : "PRESS");
	if (uinput_write_event(uinput, EV_MSC, MSC_SCAN, scan) ||
			uinput_write_event(uinput, EV_KEY, key, release ? 0 : 1)) {
		LOG(WARN, "event write failed: %s\n", errstr);
		ret = 1;
	}
	if (uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0))
		LOG(DEBUG4, "warning: syn_report failed\n");
	return ret;
}

static int event_input(int scan, int release,
		struct libevdev_uinput *uinput, struct libevdev_uinput *uinputfn)
{
	static bool fnactive = false;
	static bool fnactivated = false;
	static bool fnpressed[keymap_key_max + 1] = { };

	LOG(DEBUG3, "keystroke scan 0x%x %s\n", scan, release ? "RELEASE" : "PRESS");

	if (scan == keymap_fn_key_scan) {
		if (!release) {
			fnactive = true;
			fnactivated = false;
		} else {
			if (!fnactivated) {
				int k = keymap_direct[scan];
				if (k > 0) {
					event_emit(uinput, scan, k, 0);
					event_emit(uinput, scan, k, 1);
				}
			}
			fnactive = false;
		}
	} else if (!release && fnactive) {
		fnactivated = true;
		int k = keymap_fn[scan];
		if (k > 0) {
			fnpressed[scan] = true;
			event_emit(uinputfn, scan, k, 0);
		} else if (k < 0) {
			fnpressed[scan] = true;
			const int *combo = keymap_combo[-1 - k];
			for (; *combo > 0; combo++)
				event_emit(uinputfn, scan, *combo, 0);
		}
#ifdef FN_LEAK_UNMAPPED
		else {
			int k = keymap_direct[scan];
			if (k > 0)
				event_emit(uinput, scan, k, release);
		}
#endif
	} else if (release && fnpressed[scan]) {
		fnpressed[scan] = false;
		int k = keymap_fn[scan];
		if (k > 0) {
			event_emit(uinputfn, scan, k, 1);
		} else if (k < 0) {
			const int *combo = keymap_combo[-1 - k];
			const int *cend = combo;
			while (*cend > 0)
				cend++;
			cend--;
			for (; cend >= combo; cend--)
				event_emit(uinputfn, scan, *cend, 1);
		}
	} else {
		int k = keymap_direct[scan];
		if (k > 0)
			event_emit(uinput, scan, k, release);
	}
	return 0;
}

static int notify_backlight(void)
{
	static time_t last_notify_sent = 0;
	time_t now = time(NULL);
	if (now == last_notify_sent)
		return 0;
	last_notify_sent = now;
	FILE *f = fopen("/run/pbkbd-backlight.pid", "r");
	if (f == NULL)
		return 1;

	struct stat st;
	if (fstat(fileno(f), &st) ||
			!S_ISREG(st.st_mode) ||
			st.st_uid != 0 ||
			st.st_gid != 0 ||
			(st.st_mode & 022)) {
		/* unsafe */
		fclose(f);
		return 1;
	}
	int pid;
	fscanf(f, "%d", &pid);
	int r = kill(pid, SIGHUP);
	fclose(f);
	return r;
}

static int translate_daemon(struct libevdev *kbd,
		struct libevdev_uinput *uinput, struct libevdev_uinput *uinputfn)
{
	struct pollfd pfd = {
		.fd = libevdev_get_fd(kbd),
		.events = POLLIN
	};

	struct input_event ev;
	while (poll(&pfd, 1, 0) > 0) {
		if (libevdev_next_event(kbd, LIBEVDEV_READ_FLAG_NORMAL, &ev)
				== LIBEVDEV_READ_STATUS_SUCCESS)
			LOG(DEBUG4, "ignored event type 0x%x code 0x%x value 0x%x\n",
					(int) ev.type, (int) ev.code, (int) ev.value);
		else
			break;
	}

	int ret = 0;
	int scancode = -1;
	int scanvalue = -1;

	while (!stop) {
		if (!libevdev_has_event_pending(kbd))
			if (poll(&pfd, 1, -1) <= 0) {
				if (errno != EAGAIN && errno != EINTR) {
					LOG(ERROR, "poll failed: %s\n", errstr);
					ret = 1;
				}
				break;
			}
		if (libevdev_next_event(kbd, LIBEVDEV_READ_FLAG_NORMAL, &ev) != LIBEVDEV_READ_STATUS_SUCCESS) {
			LOG(DEBUG4, "spurious read\n");
			continue;
		}
		switch (ev.type) {
		case EV_SYN:
			LOG(DEBUG4, "SYN\n");
			if (scanvalue == 0 || scanvalue == 1)
				event_input(scancode, scanvalue == 0, uinput, uinputfn);
			scancode = -1;
			scanvalue = -1;
			notify_backlight();
			break;
		case EV_KEY:
			LOG(DEBUG4, "KEY code 0x%x %s\n", ev.code,
					ev.value == 0 ? "RELEASE" :
					ev.value == 1 ? "DEPRESSED" :
					ev.value == 2 ? "REPEAT" :
					"UNKNOWN");
			scanvalue = ev.value;
			break;
		case EV_MSC:
			if (ev.code == MSC_SCAN) {
				LOG(DEBUG4, "SCAN 0x%x\n", ev.value);
				scancode = ev.value;
				break;
			}
		default:
			LOG(DEBUG4, "kbd event type 0x%x code 0x%x value 0x%x\n",
					(int) ev.type, (int) ev.code, (int) ev.value);
		}
	}

	return ret;
}

static void priosetup(void)
{
	if (setpriority(PRIO_PROCESS, 0, -10000))
		LOG(WARN, "failed setting nice to lowest value\n");
}

static int start_daemon(void)
{
	int ret = 1;

	LOG(DEBUG, "starting daemon\n");

	LOG(DEBUG2, "setting up signal handlers\n");
	if (sigsetup()) {
		LOG(FATAL, "cannot set up signal handlers: %s\n", errstr);
		return 1;
	}

	LOG(DEBUG2, "scanning devices for keyboard\n");
	int kbdfd = scan_pbkbd();
	if (kbdfd < 0) {
		LOG(FATAL, "pixelbook keyboard not found\n");
		return 1;
	}

	struct libevdev *kbddev;
	if (libevdev_new_from_fd(kbdfd, &kbddev)) {
		LOG(FATAL, "libevdev error: %s\n", errstr);
		goto exit_close_kbdfd;
	}

	int uinputfd = -1;
	struct libevdev_uinput *uinputdev = open_uinputdev(uinputfd, "Pixelbook keyboard");
	if (uinputdev == NULL) {
		LOG(FATAL, "libevdev cannot create uinput device\n");
		goto exit_close_uinputfd;
	}

	int uinputfnfd = -1;

	struct libevdev_uinput *uinputfndev = open_uinputdev(uinputfnfd, "Pixelbook function keys");
	if (uinputfndev == NULL) {
		LOG(FATAL, "libevdev cannot create uinput device\n");
		goto exit_close_uinputfnfd;
	}

	LOG(DEBUG2, "setting up process priority\n");
	priosetup();

	LOG(DEBUG2, "grabbing device\n");
	if (libevdev_grab(kbddev, LIBEVDEV_GRAB)) {
		LOG(FATAL, "cannot grab keyboard\n");
		goto exit_close_uinputfndev;
	}

	ret = translate_daemon(kbddev, uinputdev, uinputfndev);

	//exit_ungrab:
	libevdev_grab(kbddev, LIBEVDEV_UNGRAB);
exit_close_uinputfndev:
	libevdev_uinput_destroy(uinputfndev);
exit_close_uinputfnfd:
	libevdev_uinput_destroy(uinputdev);
exit_close_uinputfd:
	libevdev_free(kbddev);
exit_close_kbdfd:
	close(kbdfd);

	return ret;
}

int main(int argc, char **argv)
{
	int c;
	while ((c = getopt(argc, argv, "vq")) > 0)
		switch (c) {
		case 'v':
			verbosity++;
			break;
		case 'q':
			verbosity--;
			break;
		default:
			LOG(ERROR, "unknown option %c\n", (char) c);
			print_help((argc == 0) ? "pbkbd" : argv[0]);
			return 1;
		}

	return start_daemon();
}
