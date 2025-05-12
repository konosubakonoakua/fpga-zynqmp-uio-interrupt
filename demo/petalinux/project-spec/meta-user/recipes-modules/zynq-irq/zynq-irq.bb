SUMMARY = "Recipe for  build an external zynq-irq Linux kernel module"
SECTION = "PETALINUX/modules"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://COPYING;md5=12f884d2ae1ff87c09e5b7ccc2c4ca7e"

inherit module

INHIBIT_PACKAGE_STRIP = "1"

SRC_URI = "file://Makefile \
           file://zynq-irq.c \
	   file://COPYING \
          "

S = "${WORKDIR}"

MODULES_DIR="${TOPDIR}/../images/linux/modules"

do_install_append() {
    mkdir -p ${MODULES_DIR}
    cp ${B}/zynq-irq.ko ${MODULES_DIR}
}

# The inherit of module.bbclass will automatically name module packages with
# "kernel-module-" prefix as required by the oe-core build environment.
