cmake_minimum_required(VERSION 3.13)
project(pixelbook-drivers VERSION 0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)
pkg_check_modules(EVDEV REQUIRED IMPORTED_TARGET libevdev)

add_executable(pbkbd "pbkbd.c")
target_link_libraries(pbkbd PRIVATE PkgConfig::EVDEV)

add_executable(pbkbd-backlight "pbkbd-backlight.c")

add_executable(pbbacklight "pbbacklight.cpp")

configure_file("pbkbd.service.in" "pbkbd.service")
configure_file("pbkbd-backlight.service.in" "pbkbd-backlight.service")
configure_file("pbbacklight.service.in" "pbbacklight.service")

install(TARGETS pbkbd pbkbd-backlight pbbacklight
	RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
install(FILES
	"${CMAKE_BINARY_DIR}/pbkbd.service"
	"${CMAKE_BINARY_DIR}/pbkbd-backlight.service"
	"${CMAKE_BINARY_DIR}/pbbacklight.service"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/systemd/system")
install(FILES 61-keyboard.hwdb
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/udev/hwdb.d")
install(FILES 61-sensor-pixelbook.rules
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/udev/rules.d")
install(FILES local-overrides.quirks README.md
	DESTINATION "${CMAKE_INSTALL_DOCDIR}")
