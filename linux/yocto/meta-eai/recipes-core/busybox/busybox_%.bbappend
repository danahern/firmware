# Enable switch_root for overlayfs preinit
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI += "file://switch_root.cfg"
