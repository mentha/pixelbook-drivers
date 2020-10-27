CFLAGS    := -Wall -Os -DNDEBUG
CXXFLAGS  := ${CFLAGS} -std=c++17

TARGETS = pbkbd pbkbd-backlight pbbacklight

all: $(TARGETS)

clean:
	-rm -f $(TARGETS)

pbkbd: pbkbd.c keymap.h
	$(CC) $(CFLAGS) -o $@ pbkbd.c $(shell pkg-config --cflags --libs libevdev)

install: all
	@echo 'Installing hwdb for keyboard'
	install -Dm644 61-keyboard.hwdb /etc/udev/hwdb.d/61-keyboard.hwdb
	systemd-hwdb update
	@echo 'Installing sensors configuration'
	install -Dm644 61-sensor-pixelbook.rules /etc/udev/rules.d/61-sensor-pixelbook.rules
	@echo 'Installing touchpad fix'
	@test -e /etc/libinput/local-overrides.quirks && echo '!!! /etc/libinput/local-overrides.quirks exists and you may have to merge it manually !!!' || install -Dm644 local-overrides.quirks /etc/libinput/local-overrides.quirks
	@echo 'Installing daemons'
	install -Dm755 pbbacklight /usr/local/sbin/pbbacklight
	install -Dm755 pbkbd /usr/local/sbin/
	install -Dm755 pbkbd-backlight /usr/local/sbin/
	install -Dm644 pbbacklight.service /usr/local/lib/systemd/system/pbbacklight.service
	install -Dm644 pbkbd-backlight.service /usr/local/lib/systemd/system/
	install -Dm644 pbkbd.service /usr/local/lib/systemd/system/
	@echo 'Enable drivers with "systemctl daemon-reload && systemctl enable pbbacklight.service pbkbd-backlight.service pbkbd.service".'
