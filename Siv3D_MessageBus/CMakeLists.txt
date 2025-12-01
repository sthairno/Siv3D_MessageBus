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

find_package(Siv3D REQUIRED)
find_package(Hiredis 1.1.0 REQUIRED)
if(SIV3D_MESSAGEBUS_BUILD_TESTS)
    find_package(GTest REQUIRED)
    find_package(Boost REQUIRED COMPONENTS filesystem system process)
endif()

get_target_property(SIV3D_INCLUDE_DIRS Siv3D::Siv3D INTERFACE_INCLUDE_DIRECTORIES)

############################################################
# Create a library
############################################################

add_library(siv3d-messagebus SHARED
    src/MessageBus.cpp
    src/RedisConnection.cpp
    src/SharedVariable.cpp
    src/SharedVariableImpl.cpp
    src/generated/HiredisLicense.cpp
)
add_library(siv3d-messagebus::siv3d-messagebus ALIAS siv3d-messagebus)

target_include_directories(siv3d-messagebus
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/ThirdParty>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        # Siv3Dは静的ライブラリしかないため、ヘッダーファイルの追加だけ行う
        ${SIV3D_INCLUDE_DIRS}
)
target_link_libraries(siv3d-messagebus
    PUBLIC
        ${HIREDIS_LIB_HIREDIS}
)

set_target_properties(siv3d-messagebus PROPERTIES
    VERSION "${SIV3D_MESSAGEBUS_VERSION}"
    SOVERSION "${SIV3D_MESSAGEBUS_SOVERSION}"
    DEBUG_POSTFIX "d"
)

############################################################
# Install
############################################################

set(SIV3D_MESSAGEBUS_INSTALL_CONFIGDIR "${CMAKE_INSTALL_LIBDIR}/cmake/Siv3D_MessageBus")

install(
    TARGETS siv3d-messagebus
    EXPORT Siv3D_MessageBusTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
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
        test/RedisConnectionPushTest.cpp
        test/RedisConnectionTest.cpp
        test/SharedVariableTest.cpp
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
            Boost::filesystem
            Boost::system
            Boost::process
    )

    add_custom_command(
        TARGET siv3d-messagebus-tests POST_BUILD
        # Siv3D の Resource() はモジュール隣の resources/ を参照するため、
        # build ディレクトリ内のバイナリから test/App/engine を指すリンクを張る
        # （App 側の実体はそのまま、コピーは行わない）
        COMMAND ${CMAKE_COMMAND} -E rm -rf $<TARGET_FILE_DIR:siv3d-messagebus-tests>/resources
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:siv3d-messagebus-tests>/resources
        COMMAND ${CMAKE_COMMAND} -E create_symlink
                ${PROJECT_SOURCE_DIR}/test/App/engine
                $<TARGET_FILE_DIR:siv3d-messagebus-tests>/resources/engine
        COMMENT "Link Siv3D runtime resources next to the test binary"
    )

    include(GoogleTest)
    gtest_discover_tests(
        siv3d-messagebus-tests
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/test/App
    )
endif()

# Example usage from another CMake project:
#   find_package(Siv3D_MessageBus CONFIG REQUIRED)
#   target_link_libraries(your_target PRIVATE Siv3D::Siv3D siv3d-messagebus::siv3d-messagebus)
