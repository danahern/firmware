# Custom inittab for cramfs read-only rootfs
# - Removes remount,rw (cramfs is read-only, overlayfs-dev handles writable layer)
# - Uses askfirst instead of ttyrun+getty (simpler, no extra dependencies)

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# Override the base inittab with our cramfs-compatible version
SRC_URI = "file://inittab"

# Don't append ttyrun-based getty entries — our inittab has askfirst directly
SERIAL_CONSOLES = ""

# Remove ttyrun dependency — we use askfirst directly
RDEPENDS:${PN}:remove = "ttyrun"

# No VT getty entries needed on embedded serial console
USE_VT = "0"
