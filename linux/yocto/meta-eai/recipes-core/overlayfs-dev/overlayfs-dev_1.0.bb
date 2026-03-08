SUMMARY = "OverlayFS writable layer for read-only rootfs (dev builds)"
DESCRIPTION = "Installs /sbin/preinit which sets up a tmpfs-backed overlayfs \
over / before exec'ing /sbin/init. Requires init=/sbin/preinit in bootargs. \
Changes are volatile (lost on reboot)."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://preinit"

S = "${WORKDIR}"

RDEPENDS:${PN} = "busybox"

do_install() {
    install -d ${D}${base_sbindir}
    install -m 0755 ${WORKDIR}/preinit ${D}${base_sbindir}/preinit
}

FILES:${PN} = "${base_sbindir}/preinit"
