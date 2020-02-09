#!/bin/bash

BINDIR="/usr/local/sbin"
SRVDIR="/usr/local/lib/systemd/system"
BINARIES='pbbacklightfs pbkbd pbkbd-backlight'
SERVICES='pbbacklightfs@ pbkbd pbkbd-backlight'

install &> /dev/null
if [[ $? -eq 127 ]]; then
	install () {
		local install_strip install_mode install_src install_dst
		install_strip=0
		install_mode=755
		while [[ "$1" = -* ]]; do
			case "$1" in
				-s) install_strip=1;;
				-m*) install_mode=${1:2};;
				*) return 1;
			esac
			shift
		done
		install_src="$1"
		install_dst="$2"
		[[ -d "$install_dst" ]] && install_dst="$install_dst/$(basename "$install_src")"
		mkdir -p "$(dirname "$install_dst")" && \
			cp -L "$install_src" "$install_dst" && \
			chmod "$install_mode" "$install_dst" && \
			([[ $install_strip -ne 0 ]] && strip -s "$install_dst" || true)
	}
fi

fatal () {
	echo "$@"
	exit 1
}

install -m644 61-keyboard.hwdb /etc/udev/hwdb.d/ || fatal "failed installing 61-keyboard.hwdb"
install -m644 61-sensor-pixelbook.rules /etc/udev/rules.d/ || fatal "failed installing 61-sensor-pixelbook.rules"

mkdir -p $BINDIR

for bin in $BINARIES
do
	install -s -m700 $bin "$BINDIR" || fatal "failed installing $bin"
done

systemctl < /dev/null &> /dev/null
if [[ $? -eq 127 ]]; then
	echo "system services are not supported without systemd"
	exit 0
fi

systemd-hwdb update || echo "warning: cannot update hwdb"

for srv in $(systemctl list-units | awk '($1 ~ /\.service$/){ print $1 }')
do
	for t in $SERVICES
	do
		match=0
		if [[ "$t" = *@ ]]; then
			if [[ "$srv" = "$t"*".service" ]]; then
				match=1
			fi
		else
			if [[ "$srv" = "$t.service" ]]; then
				match=1
			fi
		fi
		if [[ $match -ne 0 ]]; then
			systemctl stop "$srv" || echo "warning: cannot stop service $srv"
			systemctl disable "$srv" || fatal "failed disabling service $srv"
		fi
	done
done

for srv in $SERVICES
do
	install -m644 "$srv.service" "$SRVDIR" || fatal "failed installing service $srv"
done

systemctl daemon-reload || fatal "failed reloading systemd"

for srv in $SERVICES
do
	if [[ "$srv" != *@ ]]; then
		systemctl enable "$srv.service"
	fi
done

for bl in $(ls /sys/class/backlight)
do
	systemctl enable "pbbacklightfs@$bl.service"
done
