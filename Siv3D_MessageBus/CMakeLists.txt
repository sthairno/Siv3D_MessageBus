cmake_minimum_required(VERSION 3.16)
project(Siv3D_MessageBus LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Siv3D REQUIRED)
find_package(Hiredis 1.1.0 REQUIRED)
find_package(GTest REQUIRED)
find_package(Boost REQUIRED COMPONENTS filesystem system process)

get_target_property(SIV3D_INCLUDE_DIRS Siv3D::Siv3D INTERFACE_INCLUDE_DIRECTORIES)

option(SIV3D_MESSAGEBUS_BUILD_TESTS "Build MessageBus test suite" OFF)

set(SIV3D_MESSAGEBUS_SOURCES
    src/MessageBus.cpp
    src/RedisConnection.cpp
    src/generated/HiredisLicense.cpp
)

add_library(siv3d-messagebus STATIC ${SIV3D_MESSAGEBUS_SOURCES})
target_include_directories(siv3d-messagebus
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include/ThirdParty
        ${HIREDIS_INCLUDE_DIRS}
        ${SIV3D_INCLUDE_DIRS}
)
target_link_libraries(siv3d-messagebus
    PUBLIC
        ${HIREDIS_LIB_HIREDIS}
)

if(SIV3D_MESSAGEBUS_BUILD_TESTS)
    enable_testing()

    set(SIV3D_MESSAGEBUS_TEST_SOURCES
        test/Main.cpp
        test/MessageBusTest.cpp
        test/RedisConnectionPushTest.cpp
        test/RedisConnectionTest.cpp
    )

    add_executable(siv3d-messagebus-tests ${SIV3D_MESSAGEBUS_TEST_SOURCES})
    target_include_directories(siv3d-messagebus-tests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/test
            ${BOOST_INCLUDE_DIRS}
    )
    target_compile_options(siv3d-messagebus-tests PRIVATE ${GTEST_CFLAGS})
    target_link_libraries(siv3d-messagebus-tests
        PRIVATE
            siv3d-messagebus
            Siv3D::Siv3D
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

