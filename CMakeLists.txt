cmake_minimum_required(VERSION 3.13)
project(pixelbook-drivers VERSION 0.1.0 LANGUAGES C CXX)

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

set(SYSTEM_UNIT_DIR "${CMAKE_INSTALL_PREFIX}/lib/systemd/system" CACHE PATH "systemd system units dir")
set(UDEV_HWDB_DIR "${CMAKE_INSTALL_PREFIX}/lib/udev/hwdb.d" CACHE PATH "udev hwdb dir")
set(UDEV_RULES_DIR "${CMAKE_INSTALL_PREFIX}/lib/udev/rules.d" CACHE PATH "udev rules dir")

install(TARGETS pbkbd pbkbd-backlight pbbacklight
	RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
install(FILES
	"${CMAKE_BINARY_DIR}/pbkbd.service"
	"${CMAKE_BINARY_DIR}/pbkbd-backlight.service"
	"${CMAKE_BINARY_DIR}/pbbacklight.service"
	DESTINATION "${SYSTEM_UNIT_DIR}")
install(FILES 61-keyboard.hwdb
	DESTINATION "${UDEV_HWDB_DIR}")
install(FILES 61-sensor-pixelbook.rules
	DESTINATION "${UDEV_RULES_DIR}")
install(FILES local-overrides.quirks README.md
	DESTINATION "${CMAKE_INSTALL_DOCDIR}")
