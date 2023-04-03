#!/bin/bash
# Script outline to install and build kernel.
# Author: Christopher Morgan

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
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
    make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} defconfig
    sed -i '41d' ${OUTDIR}/linux-stable/scripts/dtc/dtc-lexer.l
#    sed -i '629d' /tmp/aeld/linux-stable/scripts/dtc/dtc-lexer.lex.c
    make -j4 ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} all
    make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} dtbs
    echo "Finished building kernel."
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image $OUTDIR

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
cd "$OUTDIR"
mkdir rootfs
cd rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
else
    cd busybox
fi

# TODO: Make and install busybox
make distclean
make defconfig
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
cd "$OUTDIR/rootfs"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
cp ${FINDER_APP_DIR}/deps/libs/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/.

cp ${FINDER_APP_DIR}/deps/libs/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64/.
cp ${FINDER_APP_DIR}/deps/libs/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64/.
cp ${FINDER_APP_DIR}/deps/libs/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64/.
# TODO: Make device nodes
cd "$OUTDIR/rootfs"
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/console c 5 1

# TODO: Clean and build the writer utility
cd "$OUTDIR/rootfs/home"
cp ${FINDER_APP_DIR}/deps/finder-app/writer.c . 
cp ${FINDER_APP_DIR}/deps/finder-app/Makefile .
make clean
make CROSS_COMPILE=aarch64-none-linux-gnu-
# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
mkdir ../conf
mkdir ./conf
cp ${FINDER_APP_DIR}/deps/conf/username.txt ./conf/.
cp ${FINDER_APP_DIR}/deps/conf/assignment.txt ../conf/.
cp ${FINDER_APP_DIR}/deps/finder-app/finder-test.sh .
cp ${FINDER_APP_DIR}/deps/finder-app/finder.sh .
cp ${FINDER_APP_DIR}/deps/finder-app/autorun-qemu.sh .
# TODO: Chown the root directory
sudo chown -R root "$OUTDIR/rootfs"
# TODO: Create initramfs.cpio.gz
cd "$OUTDIR/rootfs"
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd "$OUTDIR"
gzip -f initramfs.cpio
