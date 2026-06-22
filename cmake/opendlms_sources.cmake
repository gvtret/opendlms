# opendlms_sources.cmake
#
# Centralized source file lists for the OpenDLMS build.

set(OPEN_DLMS_CORE_SOURCES
    cosemlib/src/csm_array.c
    cosemlib/src/csm_association.c
    cosemlib/src/csm_axdr_codec.c
    cosemlib/src/csm_ber.c
    cosemlib/src/csm_channel.c
    cosemlib/src/csm_security.c
    cosemlib/src/csm_services.c
    cosemlib/hdlc/hdlc.c
    cosemlib/crypto/aes.c
    cosemlib/crypto/cipher.c
    cosemlib/crypto/cipher_wrap.c
    cosemlib/crypto/gcm.c
    cosemlib/crypto/sha256.c
    cosemlib/crypto/sha1.c
    cosemlib/crypto/md5.c
    cosemlib/crypto/cmac.c
    cosemlib/util/bitfield.c
    cosemlib/util/clock.c
    cosemlib/util/os_util.c
)

set(OPEN_DLMS_GOST_SOURCES
    cosemlib/crypto/kuznyechik.c
    cosemlib/crypto/kuznyechik_modes.c
    cosemlib/crypto/streebog.c
)
