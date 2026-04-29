cmake_minimum_required(VERSION 3.16)
project(Siv3D_MessageBus LANGUAGES CXX)

############################################################
# Setup
############################################################

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SIV3D_MESSAGEBUS_BUILD_TESTS "Build MessageBus test suite" OFF)

set(SIV3D_MESSAGEBUS_VERSION 0.1.0)
string(REGEX MATCH "^[0-9]+" SIV3D_MESSAGEBUS_SOVERSION "${SIV3D_MESSAGEBUS_VERSION}")
if(NOT SIV3D_MESSAGEBUS_SOVERSION)
    set(SIV3D_MESSAGEBUS_SOVERSION "${SIV3D_MESSAGEBUS_VERSION}")
endif()

# hiredis
find_package(Hiredis REQUIRED)
get_target_property(HIREDIS_INCLUDE_DIRS hiredis::hiredis INTERFACE_INCLUDE_DIRECTORIES)
list(GET HIREDIS_INCLUDE_DIRS 0 HIREDIS_INCLUDE_DIR)

# Siv3D
if(APPLE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Siv3DMac.cmake")
elseif(UNIX)
    find_package(Siv3D REQUIRED)
endif()
get_target_property(SIV3D_INCLUDE_DIRS Siv3D::Siv3D INTERFACE_INCLUDE_DIRECTORIES)

# Tests
if(SIV3D_MESSAGEBUS_BUILD_TESTS)
    find_package(GTest REQUIRED)
    find_package(Boost REQUIRED COMPONENTS process)
endif()

############################################################
# Create a library
############################################################

add_library(siv3d-messagebus STATIC
    src/MessageBus.cpp
    src/ConnectNotAllowedError.cpp
    src/InvalidNameError.cpp
    src/PlayerList.cpp
    src/RedisConnection.cpp
    src/SharedVariable.cpp
    src/SharedVariableImpl.cpp
    src/SubscriptionWorker.cpp
    src/TypeMismatchError.cpp
    src/VariableWorker.cpp
)
add_library(siv3d-messagebus::siv3d-messagebus ALIAS siv3d-messagebus)

target_include_directories(siv3d-messagebus
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/ThirdParty>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        # Siv3Dは静的ライブラリしかないため、PUBLICにせずビルドに必要なヘッダーファイルの追加だけ行う
        ${SIV3D_INCLUDE_DIRS}
)

target_link_libraries(siv3d-messagebus
    PRIVATE
        hiredis::hiredis
)

set_target_properties(siv3d-messagebus PROPERTIES
    DEBUG_POSTFIX "d"
    INTERFACE_LINK_LIBRARIES "" # target_link_librariesの設定を上書き
)

# Third-party license C++ sources are generated before compilation.
set(_GENERATED_LICENSE_DIR "${PROJECT_SOURCE_DIR}/src/generated")
set(_HIREDIS_LICENSE_CPP "${_GENERATED_LICENSE_DIR}/HiredisLicense.cpp")
set(_SQIDSCPP_LICENSE_CPP "${_GENERATED_LICENSE_DIR}/SqidsCppLicense.cpp")

add_custom_command(
    OUTPUT "${_HIREDIS_LICENSE_CPP}"
    COMMAND "${CMAKE_COMMAND}"
        -DINPUT_PATH="${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/hiredis/copyright"
        -DOUTPUT_PATH="${_HIREDIS_LICENSE_CPP}"
        -DTITLE="hiredis"
        -DFUNCTION_NAME="HiredisLicense"
        -DFORMAT="bsd3"
        -P "${PROJECT_SOURCE_DIR}/cmake/GenerateLicenseCpp.cmake"
    DEPENDS
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/hiredis/copyright"
        "${PROJECT_SOURCE_DIR}/cmake/GenerateLicenseCpp.cmake"
    VERBATIM
)

add_custom_command(
    OUTPUT "${_SQIDSCPP_LICENSE_CPP}"
    COMMAND "${CMAKE_COMMAND}"
        -DINPUT_PATH="${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/sqids-cpp/copyright"
        -DOUTPUT_PATH="${_SQIDSCPP_LICENSE_CPP}"
        -DTITLE="sqids-cpp"
        -DFUNCTION_NAME="SqidsCppLicense"
        -DFORMAT="mit"
        -P "${PROJECT_SOURCE_DIR}/cmake/GenerateLicenseCpp.cmake"
    DEPENDS
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/sqids-cpp/copyright"
        "${PROJECT_SOURCE_DIR}/cmake/GenerateLicenseCpp.cmake"
    VERBATIM
)

target_sources(siv3d-messagebus PRIVATE
    "${_HIREDIS_LICENSE_CPP}"
    "${_SQIDSCPP_LICENSE_CPP}"
)

# Merge siv3d-messagebus + hiredis into libsiv3d-messagebus[d].a
# See: https://stackoverflow.com/a/14200728
if(UNIX)
    if(APPLE)
        add_custom_command(TARGET siv3d-messagebus POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -f
                    "$<TARGET_FILE_DIR:siv3d-messagebus>/libsiv3d-messagebus_merged${CMAKE_STATIC_LIBRARY_SUFFIX}"
            COMMAND libtool -static -o
                    "$<TARGET_FILE_DIR:siv3d-messagebus>/libsiv3d-messagebus_merged${CMAKE_STATIC_LIBRARY_SUFFIX}"
                    "$<TARGET_FILE:siv3d-messagebus>"
                    "$<TARGET_FILE:hiredis::hiredis>"
            COMMAND ${CMAKE_COMMAND} -E rename
                    "$<TARGET_FILE_DIR:siv3d-messagebus>/libsiv3d-messagebus_merged${CMAKE_STATIC_LIBRARY_SUFFIX}"
                    "$<TARGET_FILE:siv3d-messagebus>"
            COMMENT "Merge siv3d-messagebus + hiredis into libsiv3d-messagebus[d].a"
            VERBATIM
        )
    else()
        add_custom_command(TARGET siv3d-messagebus POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -f
                    "$<TARGET_FILE_DIR:siv3d-messagebus>/libsiv3d-messagebus_merged${CMAKE_STATIC_LIBRARY_SUFFIX}"
            COMMAND /bin/sh -c
                    "printf 'CREATE %s\\nADDLIB %s\\nADDLIB %s\\nSAVE\\nEND\\n' \"$<TARGET_FILE_DIR:siv3d-messagebus>/libsiv3d-messagebus_merged${CMAKE_STATIC_LIBRARY_SUFFIX}\" \"$<TARGET_FILE:siv3d-messagebus>\" \"$<TARGET_FILE:hiredis::hiredis>\" | \"${CMAKE_AR}\" -M"
            COMMAND ${CMAKE_COMMAND} -E rename
                    "$<TARGET_FILE_DIR:siv3d-messagebus>/libsiv3d-messagebus_merged${CMAKE_STATIC_LIBRARY_SUFFIX}"
                    "$<TARGET_FILE:siv3d-messagebus>"
            COMMENT "Merge siv3d-messagebus + hiredis into libsiv3d-messagebus[d].a"
            VERBATIM
        )
    endif()
endif()

############################################################
# Install
############################################################

set(SIV3D_MESSAGEBUS_INSTALL_CONFIGDIR "${CMAKE_INSTALL_LIBDIR}/cmake/Siv3D_MessageBus")

install(
    TARGETS siv3d-messagebus
    EXPORT Siv3D_MessageBusTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

export(
    EXPORT Siv3D_MessageBusTargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/Siv3D_MessageBusTargets.cmake"
    NAMESPACE siv3d-messagebus::
)

install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/include/ThirdParty/MessageBus/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/MessageBus"
)

# MessageBus の公開ヘッダが <hiredis/...> を参照するため、hiredis ヘッダも同梱する
install(
    DIRECTORY "${HIREDIS_INCLUDE_DIR}/hiredis"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
    EXPORT Siv3D_MessageBusTargets
    FILE Siv3D_MessageBusTargets.cmake
    NAMESPACE siv3d-messagebus::
    DESTINATION "${SIV3D_MESSAGEBUS_INSTALL_CONFIGDIR}"
)

configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/Siv3D_MessageBusConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Siv3D_MessageBusConfig.cmake"
    INSTALL_DESTINATION "${SIV3D_MESSAGEBUS_INSTALL_CONFIGDIR}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/Siv3D_MessageBusConfigVersion.cmake"
    VERSION "${SIV3D_MESSAGEBUS_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/Siv3D_MessageBusConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/Siv3D_MessageBusConfigVersion.cmake"
    DESTINATION "${SIV3D_MESSAGEBUS_INSTALL_CONFIGDIR}"
)

############################################################
# Test
############################################################

if(SIV3D_MESSAGEBUS_BUILD_TESTS)
    enable_testing()

    add_executable(siv3d-messagebus-tests
        test/Main.cpp
        test/MessageBusTest.cpp
        test/PlayerListTest.cpp
        test/RedisConnectionPushTest.cpp
        test/RedisConnectionTest.cpp
        test/SharedVariableTest.cpp
        test/SubscriptionWorkerTest.cpp
        test/VariableWorkerTest.cpp
    )
    target_include_directories(siv3d-messagebus-tests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/test
            ${BOOST_INCLUDE_DIRS}
    )
    target_compile_options(siv3d-messagebus-tests PRIVATE ${GTEST_CFLAGS})
    target_link_libraries(siv3d-messagebus-tests
        PRIVATE
            Siv3D::Siv3D
            siv3d-messagebus
            GTest::gtest
            Boost::process
            # Boost::filesystem は Siv3D::Siv3D に含まれているため、重複を避ける
    )

    if(APPLE)
        # MacOS

        # Info.plistを設定してバンドルIDを提供
        # Siv3DのFileSystem実装がbundleIdentifierを必要とするため
        set_target_properties(siv3d-messagebus-tests PROPERTIES
            MACOSX_BUNDLE ON
            MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/test/Info.plist"
            MACOSX_BUNDLE_BUNDLE_NAME "siv3d-messagebus-tests"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.siv3d.messagebus.tests"
        )

        # test/App/engine を Resources/engine として配置
        file(GLOB_RECURSE ENGINE_RESOURCES "${PROJECT_SOURCE_DIR}/test/App/engine/*")
        foreach(resource ${ENGINE_RESOURCES})
            file(RELATIVE_PATH rel_path "${PROJECT_SOURCE_DIR}/test/App/engine" "${resource}")
            get_filename_component(rel_dir "${rel_path}" DIRECTORY)
            set_source_files_properties("${resource}" PROPERTIES
                MACOSX_PACKAGE_LOCATION "Resources/engine/${rel_dir}"
            )
        endforeach()
        target_sources(siv3d-messagebus-tests PRIVATE ${ENGINE_RESOURCES})
    elseif(UNIX)
        # Linux
        
        # Siv3D の Resource() はモジュール隣の resources/ を参照するため、
        # build ディレクトリ内のバイナリから test/App/engine を指すリンクを張る
        add_custom_command(
            TARGET siv3d-messagebus-tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -rf $<TARGET_FILE_DIR:siv3d-messagebus-tests>/resources
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:siv3d-messagebus-tests>/resources
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                    ${PROJECT_SOURCE_DIR}/test/App/engine
                    $<TARGET_FILE_DIR:siv3d-messagebus-tests>/resources/engine
            COMMENT "Link Siv3D runtime resources next to the test binary"
        )
    endif()

    include(GoogleTest)
    gtest_discover_tests(siv3d-messagebus-tests)
endif()

# Example usage from another CMake project:
#   find_package(Siv3D_MessageBus CONFIG REQUIRED)
#   target_link_libraries(your_target PRIVATE Siv3D::Siv3D siv3d-messagebus::siv3d-messagebus)
