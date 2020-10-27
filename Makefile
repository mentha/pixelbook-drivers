CFLAGS    := -Wall -Os -DNDEBUG
CXXFLAGS  := ${CFLAGS} -std=c++17

TARGETS = pbkbd pbkbd-backlight pbbacklight

all: $(TARGETS)

clean:
	-rm -f $(TARGETS)

pbkbd: pbkbd.c keymap.h
	$(CC) $(CFLAGS) -o $@ pbkbd.c $(shell pkg-config --cflags --libs libevdev)

install: all
	install -Dm755 pbbacklight /usr/local/sbin/
	install -Dm755 pbkbd /usr/local/sbin/
	install -Dm755 pbkbd-backlight /usr/local/sbin/
	install -Dm644 pbbacklight.service /usr/local/lib/systemd/system/
	install -Dm644 pbkbd-backlight.service /usr/local/lib/systemd/system/
	install -Dm644 pbkbd.service /usr/local/lib/systemd/system/
	@echo 'Enable drivers with "systemctl daemon-reload && systemctl enable pbbacklight.service pbkbd-backlight.service pbkbd.service".'
