#define _POSIX_C_SOURCE 200809L

#include <cerrno>
#include <cstdint>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <list>
#include <map>
#include <poll.h>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

using std::cerr;
using std::endl;
using std::list;
using std::map;
using std::move;
using std::string;
using std::string_view;

static bool starts_with(const string &s, const string_view &f)
{
	if (s.size() < f.size())
		return false;
	string_view v(s.data(), f.size());
	return v == f;
}

static bool ends_with(const string &s, const string_view &f)
{
	if (s.size() < f.size())
		return false;
	string_view v(s.data() + (s.size() - f.size()), f.size());
	return v == f;
}

static list<string> list_dir(const string &path)
{
	struct dir {
		DIR *d;
		dir(const string &path) {
			d = opendir(path.c_str());
			if (!d)
				throw std::system_error(errno, std::generic_category());
		}
		~dir() {
			closedir(d);
		}
		list<string> listdirs() {
			list<string> r;
			dirent *e;
			for (e = readdir(d); e; e = readdir(d)) {
				string ent(e->d_name);
				if (ent == "." || ent == "..")
					continue;
				r.push_back(move(ent));
			}
			return r;
		}
	};
	return dir(path).listdirs();
}

class SysBacklight {
public:
	string path;
	SysBacklight() {
		return;
	}
	SysBacklight(const string &p) : path(p) {
		return;
	}
	int max() {
		return readint("max_brightness");
	}
	int value() {
		return readint("brightness");
	}
	int actual() {
		return readint("actual_brightness");
	}
	double ratio() {
		return static_cast<double>(value()) / max();
	}

	string fullpath(const string &name) {
		if (ends_with(path, "/"))
			return path + name;
		return path + "/" + name;
	}
	int readint(const string &name) {
		std::ifstream s(fullpath(name));
		if (!s.is_open())
			throw std::runtime_error("");
		int r;
		s >> r;
		return r;
	}

	static list<string> enumerate() {
		list<string> r;
		const string syspath = "/sys/class/backlight/";
		for (auto e: list_dir(syspath))
			r.push_back(syspath + e);
		return r;
	}
};

class DPBacklight {
	int fd;
public:
	DPBacklight(const string &path) {
		fd = open(path.c_str(), O_RDWR | O_NOCTTY);
		if (fd < 0)
			throw std::system_error(errno, std::generic_category());
	}
	~DPBacklight() {
		close(fd);
	}
	void setup() {
		uint8_t b;
		/* 0x721 MODE_SET_REGISTER */
		if (pread(fd, &b, 1, 0x721) != 1)
			throw std::system_error(errno, std::generic_category());
		b &= 0xf8;
		b |= 0x2;
		if (pwrite(fd, &b, 1, 0x721) != 1)
			throw std::system_error(errno, std::generic_category());
		/* 0x724 PWMGEN_BIT_COUNT */
		b = 0x10; /* magic number that works */
		if (pwrite(fd, &b, 1, 0x724) != 1)
			throw std::system_error(errno, std::generic_category());
		/* 0x728 BACKLIGHT_FREQ_SET */
		b = 0x01;
		if (pwrite(fd, &b, 1, 0x728) != 1)
			throw std::system_error(errno, std::generic_category());
	}
	void set(int value) {
		if (value < 0)
			value = 0;
		if (value > 0xffff)
			value = 0xffff;
		uint8_t b[2];
		b[0] = value >> 8;
		b[1] = value & 0xff;
		/* 0x722 BRIGHTNESS_MSB */
		if (pwrite(fd, b, 2, 0x722) != 2)
			throw std::system_error(errno, std::generic_category());
	}
	int get() {
		uint8_t buf[2];
		if (pread(fd, buf, 2, 0x722) != 2)
			throw std::system_error(errno, std::generic_category());
		return (buf[0] << 8) | buf[1];
	}
};

class PBBacklight {
public:
	string dpaux;
	PBBacklight() {
		const string dpauxpath = "/sys/class/drm_dp_aux_dev/";
		const std::regex re_link("/card[0-9]+-eDP-[0-9]+/");
		for (auto e: list_dir(dpauxpath)) {
			if (!starts_with(e, "drm_dp_aux"))
				continue;
			auto fp = dpauxpath + e;
			string linktgt;
			linktgt.resize(1000);
			auto tl = readlink(fp.c_str(), linktgt.data(), linktgt.size());
			if (tl <= 0)
				throw std::system_error(errno, std::generic_category());
			linktgt.resize(tl - 1);
			std::smatch m;
			regex_search(linktgt, m, re_link);
			if (m.empty())
				continue;
			dpaux = "/dev/" + e;
			return;
		}
		throw std::runtime_error("no suitable DP AUX device found");
	}
	void set(int value) {
		DPBacklight bl(dpaux);
		bl.setup();
		bl.set(value);
	}
	int get() {
		DPBacklight bl(dpaux);
		bl.setup();
		return bl.get();
	}
};

class PBBLManager {
public:
	constexpr static double perstep = 0.05;
	constexpr static int interval = 10; /* step interval ms */
	constexpr static int max_bri = 0xffff;
	constexpr static int min_bri = 300;

	PBBacklight pbbl;

	double cur_bri;
	double tgt_bri;

	static int absbri(double v) {
		return min_bri + v * (max_bri - min_bri);
	}
	PBBLManager() {
		cur_bri = absbri(0.5);
		tgt_bri = absbri(0.5);
	}
	void reset(double v) {
		tgt_bri = absbri(v);
		cur_bri = tgt_bri;
		pbbl.set(cur_bri + 0.5);
	}
	void update(double v) {
		tgt_bri = absbri(v);
	}
	unsigned step() {
		double d = tgt_bri - cur_bri;
		double nextbri = tgt_bri;
		unsigned ret = 0;
		if (std::abs(d) > 2) {
			nextbri = cur_bri + perstep * d;
			ret = interval;
		}
		cur_bri = nextbri;
		pbbl.set(cur_bri + 0.5);
		return ret;
	}
};

class BLProxy {
public:
	PBBLManager pbbl;
	map<int, SysBacklight> blmap;
	int watcher;

	BLProxy() {
		watcher = inotify_init1(IN_NONBLOCK);
		if (watcher < 0)
			throw std::system_error(errno, std::generic_category());
	}
	~BLProxy() {
		close(watcher);
	}
	void add(const string &path) {
		SysBacklight b(path);
		int desc = inotify_add_watch(watcher, b.fullpath("brightness").c_str(), IN_MODIFY);
		if (desc < 0)
			throw std::system_error(errno, std::generic_category());
		blmap[desc] = move(b);
	}
	double getbri() {
		double a = 0;
		unsigned n = 0;
		for (auto i: blmap) {
			a += i.second.ratio();
			n += 1;
		}
		return a / n;
	}
	SysBacklight *waitmodify(unsigned mstimeout) {
		struct pollfd pfd = {
			.fd = watcher,
			.events = POLLIN
		};
		int r = poll(&pfd, 1, mstimeout);
		if (r < 0)
			throw std::system_error(errno, std::generic_category());
		if (r == 0)
			return nullptr;
		char buf[sizeof(inotify_event) + PATH_MAX + 1];
		read(watcher, buf, sizeof(buf));
		auto *ev = reinterpret_cast<inotify_event *>(buf);
		return &blmap[ev->wd];
	}
	void mainloop() {
		pbbl.reset(getbri());
		for (;;) {
			int ms = pbbl.step();
			if (ms == 0)
				ms = 1000;
			auto *bl = waitmodify(ms);
			if (bl)
				pbbl.update(bl->ratio());
		}
	}
};

int main(int argc, char **argv)
{
	try {
		BLProxy p;
		if (argc >= 2) {
			for (int i = 1; i < argc; i++)
				p.add(argv[i]);
		} else {
			for (auto v: SysBacklight::enumerate())
				p.add(v);
		}

		p.mainloop();
	} catch (std::exception &e) {
		cerr << "fatal error: " << e.what() << endl;
		return 1;
	}

	return 0;
}
