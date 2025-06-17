#!/bin/sh
if test -f "./root/bin/busybox"; then
    echo "Already have busybox exists."
else
    echo "Download busybox"
    curl -L 'https://www.busybox.net/downloads/binaries/1.26.2-defconfig-multiarch/busybox-x86_64' > ./root/bin/busybox
    chmod +x ./root/bin/busybox
fi

echo "Creating symlinks"
cd ./root/bin/
ln -s ./busybox ./sh
ln -s ./busybox ./mount
ln -s ./busybox ./ls
ln -s ./busybox ./id
ln -s ./busybox ./shutdown
ln -s ./busybox ./ps
ln -s ./busybox ./poweroff
ln -s ./busybox ./cat
ln -s ./busybox ./uname
ln -s ./busybox ./grep
ln -s ./busybox ./chmod
ln -s ./busybox ./su
cd ../../