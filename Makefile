cmakedir = build

all: install

generate:
	cmake -B$(cmakedir) . -DCMAKE_BUILD_TYPE=Release

build: generate
	cmake --build $(cmakedir)

install: build
	cmake --install $(cmakedir)
