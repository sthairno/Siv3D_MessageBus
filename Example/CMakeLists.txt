cmake_minimum_required(VERSION 3.16)
project(Siv3D_MessageBus_Example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Siv3D REQUIRED)
find_package(Siv3D_MessageBus REQUIRED)

add_executable(Siv3D_MessageBus_Example
    Main.cpp
)

target_link_libraries(Siv3D_MessageBus_Example
    PUBLIC
        Siv3D::Siv3D
        siv3d-messagebus::siv3d-messagebus
)

add_custom_command(
        TARGET Siv3D_MessageBus_Example POST_BUILD
        # Siv3D の Resource() はモジュール隣の resources/ を参照するため、
        # build ディレクトリ内のバイナリから App/engine を指すリンクを張る
        COMMAND ${CMAKE_COMMAND} -E rm -rf $<TARGET_FILE_DIR:Siv3D_MessageBus_Example>/resources
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:Siv3D_MessageBus_Example>/resources
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${PROJECT_SOURCE_DIR}/App/engine
        $<TARGET_FILE_DIR:Siv3D_MessageBus_Example>/resources/engine
        COMMENT "Link Siv3D runtime resources next to the application binary"
)
