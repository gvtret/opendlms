cmake_minimum_required(VERSION 3.10)
project(cosemlib LANGUAGES C)

add_library(${PROJECT_NAME} STATIC)

target_sources(${PROJECT_NAME} PRIVATE
    src/csm_array.c
    src/csm_association.c
    src/csm_axdr_codec.c
    src/csm_ber.c
    src/csm_channel.c
    src/csm_security.c
    src/csm_services.c
    hdlc/hdlc.c
    crypto/aes.c
    crypto/cipher.c
    crypto/cipher_wrap.c
    crypto/gcm.c
    crypto/sha256.c
    crypto/sha1.c
    crypto/md5.c
    crypto/cmac.c
    util/bitfield.c
    util/clock.c
    util/os_util.c
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/util
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto
    ${CMAKE_CURRENT_SOURCE_DIR}/hdlc
)
