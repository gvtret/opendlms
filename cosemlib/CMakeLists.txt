cmake_minimum_required(VERSION 3.10)
project(cosemlib VERSION 1.1.0 LANGUAGES C)

option(COSEMLIB_SHARED "Build as shared library" OFF)

if(COSEMLIB_SHARED)
    add_library(${PROJECT_NAME} SHARED)
else()
    add_library(${PROJECT_NAME} STATIC)
endif()

target_sources(${PROJECT_NAME} PRIVATE
    src/csm_array.c
    src/csm_association.c
    src/csm_axdr_codec.c
    src/csm_ber.c
    src/csm_block_transfer.c
    src/csm_channel.c
    src/csm_framing.c
    src/csm_keyring.c
    src/csm_security.c
    src/csm_security_suite.c
    src/csm_server.c
    src/csm_services.c
    src/csm_transport_tcp.c
    hdlc/hdlc.c
    crypto/aes.c
    crypto/cipher.c
    crypto/cipher_wrap.c
    crypto/gcm.c
    crypto/sha256.c
    crypto/sha1.c
    crypto/md5.c
    crypto/cmac.c
    crypto/kuznyechik.c
    crypto/kuznyechik_modes.c
    crypto/streebog.c
    util/bitfield.c
    util/clock.c
    util/os_util.c
    ic/db_cosem_ic.c
    ic/db_cosem_ic_class.c
    ic/db_cosem_ic_registry.c
    ic/db_cosem_data.c
    ic/db_cosem_register.c
    ic/db_cosem_ext_register.c
    ic/db_cosem_demand_register.c
    ic/db_cosem_clock_util.c
    ic/db_cosem_ic_profile.c
    ic/db_cosem_ic_push.c
    ic/db_cosem_ic_security_setup.c
    ic/db_cosem_ic_limiter.c
    ic/db_cosem_ic_association.c
    ic/db_cosem_ic_schedule.c
    ic/db_cosem_ic_special_days.c
    ic/db_cosem_ic_activity_calendar.c
    ic/db_cosem_ic_register_monitor.c
    ic/db_cosem_ic_single_action_schedule.c
    ic/db_cosem_ic_script_table.c
    ic/db_cosem_ic_comms.c
    ic/db_cosem_ic_image_transfer.c
    ic/db_cosem_ic_disconnect.c
    ic/db_cosem_ic_sap_assignment.c
    ic/db_cosem_ic_register_activation.c
    ic/db_cosem_ic_utility_tables.c
    ic/db_cosem_ic_compact_data.c
    ic/db_cosem_ic_register_table.c
    ic/db_cosem_ic_status_mapping.c
    ic/db_cosem_ic_parameter_monitor.c
    ic/db_cosem_ic_arbitrator.c
    ic/db_cosem_ic_sensor_manager.c
    ic/db_cosem_ic_data_protection.c
    ic/db_cosem_ic_profile_filter.c
    ic/db_cosem_ic_table_manager.c
    model/csm_model_catalog.c
    model/csm_model_instance.c
    model/csm_model_object_list.c
    profile/csm_spodes.c
    profile/csm_spodus.c
    profile/csm_profile_capture.c
    profile/csm_profile_row.c
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/util>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/crypto>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hdlc>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ic>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/model>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/profile>
        $<INSTALL_INTERFACE:include/opendlms>
)

set(COSEMLIB_PUBLIC_HEADERS
    src/cosemlib.h
    src/csm_array.h
    src/csm_association.h
    src/csm_axdr_codec.h
    src/csm_ber.h
    src/csm_block_transfer.h
    src/csm_channel.h
    src/csm_config.h
    src/csm_definitions.h
    src/csm_framing.h
    src/csm_keyring.h
    src/csm_security.h
    src/csm_security_suite.h
    src/csm_server.h
    src/csm_services.h
    src/csm_transport.h
    src/csm_transport_tcp.h
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${COSEMLIB_PUBLIC_HEADERS}"
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

if(COSEMLIB_SHARED)
    target_compile_definitions(${PROJECT_NAME} PRIVATE COSEMLIB_SHARED)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        DEFINE_SYMBOL COSEMLIB_EXPORT
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
endif()

# ── Install ─────────────────────────────────────────────────────────────────

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    EXPORT OpenDLMSConfig
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/opendlms
)

install(EXPORT OpenDLMSConfig
    NAMESPACE OpenDLMS::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/opendlms
)

# ── pkg-config ──────────────────────────────────────────────────────────────

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/opendlms.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/opendlms.pc
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/opendlms.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
