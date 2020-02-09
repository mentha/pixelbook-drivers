CFLAGS    := -Wall -Os -DNDEBUG

TARGETS = pbkbd pbkbd-backlight pbbacklightfs

all: $(TARGETS)

clean:
	-rm -f $(TARGETS)

install: all
	./install.sh

pbkbd: pbkbd.c keymap.h
	$(CC) $(CFLAGS) -o $@ pbkbd.c $(shell pkg-config --cflags --libs libevdev)

pbbacklightfs: pbbacklightfs.c fuse-sysfs.c edp-backlight.c fuse-sysfs.h edp-backlight.h
	$(CC) $(CFLAGS) -o $@ pbbacklightfs.c fuse-sysfs.c edp-backlight.c $(shell pkg-config --cflags --libs fuse)
