SUMMARY = "PicoClaw - Ultra-lightweight AI assistant gateway"
DESCRIPTION = "Self-hosted AI assistant gateway for OpenClaw. \
Pre-compiled Go binary, statically linked, no runtime dependencies."
HOMEPAGE = "https://github.com/sipeed/picoclaw"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://picoclaw \
           file://config.json \
           file://SOUL.md \
           file://IDENTITY.md \
           file://picoclaw-init"

S = "${WORKDIR}"

# Pre-compiled static binary — skip arch/OS checks
INSANE_SKIP:${PN} = "already-stripped ldflags file-rdeps"

# No build step — binary is pre-compiled
do_compile[noexec] = "1"

inherit update-rc.d

INITSCRIPT_NAME = "picoclaw"
INITSCRIPT_PARAMS = "defaults 95"

do_install() {
    # Binary
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/picoclaw ${D}${bindir}/picoclaw

    # Default config (user copies to config.json and edits)
    install -d ${D}${sysconfdir}/picoclaw
    install -m 0644 ${WORKDIR}/config.json ${D}${sysconfdir}/picoclaw/config.json.default

    # Workspace with personality templates
    install -d ${D}/var/lib/picoclaw/workspace
    install -d ${D}/var/lib/picoclaw/workspace/memory
    install -m 0644 ${WORKDIR}/SOUL.md ${D}/var/lib/picoclaw/workspace/SOUL.md
    install -m 0644 ${WORKDIR}/IDENTITY.md ${D}/var/lib/picoclaw/workspace/IDENTITY.md

    # Init script
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/picoclaw-init ${D}${sysconfdir}/init.d/picoclaw

    # Log directory
    install -d ${D}/var/log
}

FILES:${PN} = "${bindir}/picoclaw \
               ${sysconfdir}/picoclaw \
               ${sysconfdir}/init.d/picoclaw \
               /var/lib/picoclaw \
               /var/log"

CONFFILES:${PN} = "${sysconfdir}/picoclaw/config.json.default"
