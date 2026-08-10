# CMakeLists.txt for afros-harmonygate
# =============================================================================
# afros-harmonygate is AfriOS's HarmonyOS compatibility layer. It is shipped
# as a single shared library, libafros-harmonygate.so, that bundles:
#
#   * distributed/device_manager/  — device discovery, capability
#                                    advertisement, trust management,
#                                    monitoring.
#   * distributed/data_sync/       — distributed data manager, sync engine,
#                                    versioning, conflict resolution.
#   * distributed/softbus/         — HarmonyOS "soft bus" with its four
#                                    sub-layers: discovery (BLE/mDNS/CoAP),
#                                    connection (TCP/BT/WiFi-Direct),
#                                    authentication, transmission (file +
#                                    stream).
#   * distributed/hardware_sharing/— camera/storage/sensor sharing across
#                                    HarmonyOS-linked devices.
#   * ability/                     — HarmonyOS "Ability" runtime (lifecycle,
#                                    manager, context) and Want (intent)
#                                    model, used to launch HarmonyOS apps.
#
# The C side (distributed/*) is plain C11; the ability/ side is C++17.
# =============================================================================

set(AFROS_HARMONYGATE_C_SOURCES
    # --- Device manager -----------------------------------------------------
    distributed/device_manager/device_discovery.c
    distributed/device_manager/capability_manager.c
    distributed/device_manager/trust_manager.c
    distributed/device_manager/device_monitor.c

    # --- Data sync ----------------------------------------------------------
    distributed/data_sync/distributed_data_mgr.c
    distributed/data_sync/sync_engine.c
    distributed/data_sync/versioning.c
    distributed/data_sync/conflict_resolver.c

    # --- Softbus: discovery -------------------------------------------------
    distributed/softbus/discovery/ble_discovery.c
    distributed/softbus/discovery/mdns_discovery.c
    distributed/softbus/discovery/coap_discovery.c

    # --- Softbus: connection ------------------------------------------------
    distributed/softbus/connection/tcp_connection.c
    distributed/softbus/connection/bluetooth_connection.c
    distributed/softbus/connection/wifi_direct.c

    # --- Softbus: authentication -------------------------------------------
    distributed/softbus/authentication/device_auth.c

    # --- Softbus: transmission ---------------------------------------------
    distributed/softbus/transmission/file_transmission.c
    distributed/softbus/transmission/stream_transmission.c

    # --- Hardware sharing ---------------------------------------------------
    distributed/hardware_sharing/camera_sharing.c
    distributed/hardware_sharing/storage_sharing.c
    distributed/hardware_sharing/sensor_sharing.c
)

set(AFROS_HARMONYGATE_CXX_SOURCES
    # --- Ability runtime ----------------------------------------------------
    ability/ability_runtime/ability_manager.cpp
    ability/ability_runtime/ability_context.cpp
    ability/ability_runtime/ability_lifecycle.cpp

    # --- Want (intent) ------------------------------------------------------
    ability/want/want.cpp
    ability/want/want_params.cpp
)

add_library(afros-harmonygate SHARED
    ${AFROS_HARMONYGATE_C_SOURCES}
    ${AFROS_HARMONYGATE_CXX_SOURCES}
)

set_target_properties(afros-harmonygate PROPERTIES
    C_STANDARD 11
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME "afros-harmonygate"
)

target_include_directories(afros-harmonygate PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# -lpthread: the softbus transport layer, sync engine and ability runtime
# all use background threads (discovery beacons, TCP accept loops, etc.).
if(NOT TARGET Threads::Threads)
    find_package(Threads REQUIRED)
endif()
target_link_libraries(afros-harmonygate PUBLIC Threads::Threads)

# --- Install rules -----------------------------------------------------------
install(TARGETS afros-harmonygate
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
install(DIRECTORY include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h")
