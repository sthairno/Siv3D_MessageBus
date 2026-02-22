# Patched version: fix/disconnect_during_pubsub_mode (sub.replies in disconnect condition)
# https://github.com/sthairno/hiredis/tree/fix/disconnect_during_pubsub_mode
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sthairno/hiredis
    REF cd84e6ade9b163bd63392dd83849fa389e08d2a8
    SHA512 4bfd611f636922c08da939226aa9dcd98653d31d918dd864cd4cf3ff23c49181f9abbd4630409e8c8d0e16640001a10687edd5d7bb966073f5a9207c2ff77e05
    HEAD_REF fix/disconnect_during_pubsub_mode
    PATCHES
        fix-timeval.patch
        fix-ssize_t.patch
        support-static.patch
        fix-cmake-conf-install-dir.patch
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        ssl     ENABLE_SSL
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS ${FEATURE_OPTIONS}
      -DDISABLE_TESTS=ON
      -DBUILD_SHARED_LIBS=OFF
)

vcpkg_cmake_install()

vcpkg_copy_pdbs()

vcpkg_fixup_pkgconfig()

vcpkg_cmake_config_fixup()
if("ssl" IN_LIST FEATURES)
    vcpkg_cmake_config_fixup(PACKAGE_NAME hiredis_ssl CONFIG_PATH share/hiredis_ssl)
endif()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

# Handle copyright
file(INSTALL "${SOURCE_PATH}/COPYING" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
