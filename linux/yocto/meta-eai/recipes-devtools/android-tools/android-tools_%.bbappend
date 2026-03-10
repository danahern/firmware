FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://adb_auth_client_stub.c"

# Replace adbd auth with stub (no OpenSSL) and remove -lcrypto from adbd linker flags.
# OpenSSL stays in DEPENDS because the adb client tool still needs it.
# Only adbd is installed on target (android-tools-adbd package), so libcrypto
# won't be pulled into the rootfs.
do_configure:prepend() {
    cp ${WORKDIR}/adb_auth_client_stub.c ${S}/system/core/adb/adb_auth_client.c
    sed -i 's/-lresolv -lcrypto//' ${B}/adbd.mk
}
