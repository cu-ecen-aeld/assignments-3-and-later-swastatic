#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p rootfs
cd rootfs
mkdir -p bin conf dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log
mkdir -p home/conf

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install
cd ${OUTDIR}/rootfs

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
SYSROOT=`${CROSS_COMPILE}gcc --print-sysroot`
echo sysroot is $SYSROOT

# Program interpreter placed in /lib directory
# Shared Libraries placed in /lib64 directory (since architecture is 64 bit)
cp $SYSROOT/lib/ld-linux-aarch64.so.1 lib/
cp $SYSROOT/lib64/libm.so.6 lib64/
cp $SYSROOT/lib64/libresolv.so.2 lib64/
cp $SYSROOT/lib64/libc.so.6 lib64/

# TODO: Make device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/console c 5 1

# TODO: Clean and build the writer utility
make -C ${FINDER_APP_DIR} clean CROSS_COMPILE=${CROSS_COMPILE}
make -C ${FINDER_APP_DIR} all CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp ${FINDER_APP_DIR}/finder.sh home/
cp ${FINDER_APP_DIR}/finder-test.sh home/
cp ${FINDER_APP_DIR}/autorun-qemu.sh home/
cp ${FINDER_APP_DIR}/writer home/
cp ${FINDER_APP_DIR}/start-qemu-app.sh .
cp ${FINDER_APP_DIR}/../conf/username.txt home/conf/
cp ${FINDER_APP_DIR}/../conf/assignment.txt conf/

# TODO: Chown the root directory
chown -R root:root *

# TODO: Create initramfs.cpio.gz
#cd ${OUTDIR}/rootfs
pwd
find . | cpio -H newc -ov --owner root:root > ../initramfs.cpio
cd ../
gzip initramfs.cpio
mkimage -A arm -O linux -T ramdisk -d initramfs.cpio.gz uRamdisk
