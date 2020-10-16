CFLAGS    := -Wall -Os -DNDEBUG
CXXFLAGS  := ${CFLAGS} -std=c++17

TARGETS = pbkbd pbkbd-backlight pbbacklight

all: $(TARGETS)

clean:
	-rm -f $(TARGETS)

install: all
	./install.sh

pbkbd: pbkbd.c keymap.h
	$(CC) $(CFLAGS) -o $@ pbkbd.c $(shell pkg-config --cflags --libs libevdev)
