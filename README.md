# pixelbook-drivers
userspace drivers for pixelbooks running vanilla kernel

# Features

 * Backlight control for bulitin screen
 * Key combos for missing keys
 * Keyboard backlight adjusting to ambient light

## Partially working features

 * Sensors
   - GNOME does not filter out noise from sensors, automatic brightness and automatic screen rotation is unreliable

## Completely unusable devices

 * Speaker and audio jack
   - The in-tree driver correctly detects the DSP with firmware installed, but always timeout initializing it
   - In-tree driver contain bugs, hanging random user process accessing alsa

# Installing

Run `make install` as root, then `systemd-hwdb update`, and enable `pb{backlight,kbd}.service`.

## Kernel options

 * eMMC
   - `CONFIG_MMC_SDHCI_PCI`
 * Backlight control
   - `CONFIG_DRM_DP_AUX_CHARDEV`
 * Keyboard backlight
   - `CONFIG_CROS_KBD_LED_BACKLIGHT`
 * Sensors
   - `CONFIG_IIO_CROS_EC_SENSORS`
 * Sound (Not working)
   - `CONFIG_SND_SOC_INTEL_KBL_RT5663_RT5514_MAX98927_MACH`

## Prerequisites

 * A kernel with required kernel option selected
 * CMake
 * Development tools
 * Development headers for
   - libevdev
