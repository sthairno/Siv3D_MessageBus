# Siv3DMac.cmake
# Mac向けSiv3D用のCMake設定ファイル
# cmake/siv3d_mac ディレクトリに展開されたSiv3Dを使用する
# 存在しない場合は自動的にダウンロード・展開を行う

set(SIV3D_MAC_ROOT "${CMAKE_CURRENT_LIST_DIR}/siv3d_mac")
set(SIV3D_MAC_INCLUDE_DIR "${SIV3D_MAC_ROOT}/include")
set(SIV3D_MAC_LIB_DIR "${SIV3D_MAC_ROOT}/lib/macOS")
set(SIV3D_MAC_VERSION "0.6.16")
set(SIV3D_MAC_ZIP_URL "https://siv3d.jp/downloads/Siv3D/siv3d_v${SIV3D_MAC_VERSION}_macOS.zip")
set(SIV3D_MAC_ZIP_NAME "siv3d_v${SIV3D_MAC_VERSION}_macOS.zip")
set(SIV3D_MAC_EXTRACTED_DIR_NAME "siv3d_v${SIV3D_MAC_VERSION}_macOS")

# Siv3Dが既に展開されているか確認
set(SIV3D_ALREADY_EXTRACTED FALSE)
if(EXISTS "${SIV3D_MAC_INCLUDE_DIR}/Siv3D.hpp" AND EXISTS "${SIV3D_MAC_LIB_DIR}/libSiv3D.a")
    set(SIV3D_ALREADY_EXTRACTED TRUE)
    message(STATUS "Siv3D already extracted at: ${SIV3D_MAC_ROOT}")
endif()

# Siv3Dが存在しない場合、ダウンロード・展開を実行
if(NOT SIV3D_ALREADY_EXTRACTED)
    message(STATUS "Siv3D not found. Downloading and extracting...")
    
    # 一時ディレクトリの設定
    set(SIV3D_DOWNLOAD_DIR "${CMAKE_CURRENT_BINARY_DIR}/siv3d_download")
    set(SIV3D_ZIP_PATH "${SIV3D_DOWNLOAD_DIR}/${SIV3D_MAC_ZIP_NAME}")
    set(SIV3D_EXTRACT_DIR "${SIV3D_DOWNLOAD_DIR}/extracted")
    
    # ダウンロードディレクトリを作成
    file(MAKE_DIRECTORY "${SIV3D_DOWNLOAD_DIR}")
    file(MAKE_DIRECTORY "${SIV3D_EXTRACT_DIR}")
    
    # ZIPファイルのダウンロード（存在しない場合のみ）
    if(NOT EXISTS "${SIV3D_ZIP_PATH}")
        message(STATUS "Downloading Siv3D from: ${SIV3D_MAC_ZIP_URL}")
        file(DOWNLOAD
            "${SIV3D_MAC_ZIP_URL}"
            "${SIV3D_ZIP_PATH}"
            SHOW_PROGRESS
            STATUS download_status
        )
        
        list(GET download_status 0 status_code)
        if(NOT status_code EQUAL 0)
            list(GET download_status 1 error_message)
            message(FATAL_ERROR
                "Failed to download Siv3D: ${error_message}\n"
                "URL: ${SIV3D_MAC_ZIP_URL}"
            )
        endif()
        message(STATUS "Download completed: ${SIV3D_ZIP_PATH}")
    else()
        message(STATUS "ZIP file already exists: ${SIV3D_ZIP_PATH}")
    endif()
    
    # ZIPファイルの展開
    message(STATUS "Extracting Siv3D...")
    execute_process(
        COMMAND unzip -q "${SIV3D_ZIP_PATH}" -d "${SIV3D_EXTRACT_DIR}"
        RESULT_VARIABLE unzip_result
        OUTPUT_QUIET
        ERROR_QUIET
    )
    
    if(NOT unzip_result EQUAL 0)
        message(FATAL_ERROR
            "Failed to extract Siv3D ZIP file.\n"
            "ZIP path: ${SIV3D_ZIP_PATH}\n"
            "Extract dir: ${SIV3D_EXTRACT_DIR}"
        )
    endif()
    
    # 展開されたディレクトリを確認
    set(SIV3D_EXTRACTED_ROOT "${SIV3D_EXTRACT_DIR}/${SIV3D_MAC_EXTRACTED_DIR_NAME}")
    if(NOT EXISTS "${SIV3D_EXTRACTED_ROOT}")
        message(FATAL_ERROR
            "Extracted directory not found: ${SIV3D_EXTRACTED_ROOT}\n"
            "Please check the ZIP file structure."
        )
    endif()
    
    # cmake/siv3d_mac/ ディレクトリを作成
    file(MAKE_DIRECTORY "${SIV3D_MAC_ROOT}")
    
    # 展開されたファイルをcmake/siv3d_mac/にコピー
    message(STATUS "Copying Siv3D files to: ${SIV3D_MAC_ROOT}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SIV3D_EXTRACTED_ROOT}"
            "${SIV3D_MAC_ROOT}"
        RESULT_VARIABLE copy_result
    )
    
    if(NOT copy_result EQUAL 0)
        message(FATAL_ERROR
            "Failed to copy Siv3D files to: ${SIV3D_MAC_ROOT}"
        )
    endif()
    
    # 展開結果の確認
    if(NOT EXISTS "${SIV3D_MAC_INCLUDE_DIR}/Siv3D.hpp")
        message(FATAL_ERROR
            "Siv3D header files not found after extraction at: ${SIV3D_MAC_INCLUDE_DIR}"
        )
    endif()
    
    if(NOT EXISTS "${SIV3D_MAC_LIB_DIR}/libSiv3D.a")
        message(FATAL_ERROR
            "Siv3D library not found after extraction at: ${SIV3D_MAC_LIB_DIR}"
        )
    endif()
    
    message(STATUS "Siv3D extraction completed successfully")
endif()

# Siv3D::Siv3D ターゲットを作成（IMPORTED STATIC LIBRARY）
if(NOT TARGET Siv3D::Siv3D)
    add_library(Siv3D::Siv3D STATIC IMPORTED)
    
    # インクルードディレクトリを設定
    set_target_properties(Siv3D::Siv3D PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SIV3D_MAC_INCLUDE_DIR}"
    )
    
    # メインライブラリのパスを設定
    set_target_properties(Siv3D::Siv3D PROPERTIES
        IMPORTED_LOCATION "${SIV3D_MAC_LIB_DIR}/libSiv3D.a"
    )
    
    # Siv3Dの依存ライブラリをリンク
    # 必要なフレームワークとライブラリを設定
    target_link_libraries(Siv3D::Siv3D INTERFACE
        "${SIV3D_MAC_LIB_DIR}/libSiv3D.a"
        "${SIV3D_MAC_LIB_DIR}/boost/libboost_filesystem.a"
        "${SIV3D_MAC_LIB_DIR}/freetype/libfreetype.a"
        "${SIV3D_MAC_LIB_DIR}/harfbuzz/libharfbuzz.a"
        "${SIV3D_MAC_LIB_DIR}/libgif/liblibgif.a"
        "${SIV3D_MAC_LIB_DIR}/libjpeg-turbo/libturbojpeg.a"
        "${SIV3D_MAC_LIB_DIR}/libogg/libogg.a"
        "${SIV3D_MAC_LIB_DIR}/libpng/libpng16.a"
        "${SIV3D_MAC_LIB_DIR}/libtiff/libtiff.a"
        "${SIV3D_MAC_LIB_DIR}/libvorbis/libvorbis.a"
        "${SIV3D_MAC_LIB_DIR}/libvorbis/libvorbisenc.a"
        "${SIV3D_MAC_LIB_DIR}/libvorbis/libvorbisfile.a"
        "${SIV3D_MAC_LIB_DIR}/libwebp/libwebp.a"
        "${SIV3D_MAC_LIB_DIR}/opencv/libopencv_core.a"
        "${SIV3D_MAC_LIB_DIR}/opencv/libopencv_imgcodecs.a"
        "${SIV3D_MAC_LIB_DIR}/opencv/libopencv_imgproc.a"
        "${SIV3D_MAC_LIB_DIR}/opencv/libopencv_objdetect.a"
        "${SIV3D_MAC_LIB_DIR}/opencv/libopencv_photo.a"
        "${SIV3D_MAC_LIB_DIR}/opencv/libopencv_videoio.a"
        "${SIV3D_MAC_LIB_DIR}/opus/libopus.a"
        "${SIV3D_MAC_LIB_DIR}/opus/libopusfile.a"
        "${SIV3D_MAC_LIB_DIR}/zlib/libzlib.a"
        "-framework" "Cocoa"
        "-framework" "OpenGL"
        "-framework" "Metal"
        "-framework" "MetalKit"
        "-framework" "QuartzCore"
        "-framework" "AudioToolbox"
        "-framework" "AVFoundation"
        "-framework" "CoreAudio"
        "-framework" "CoreVideo"
        "-framework" "IOKit"
        "-framework" "Carbon"
        "-framework" "ForceFeedback"
        "-framework" "GameController"
        "-framework" "CoreHaptics"
    )
endif()

message(STATUS "Found Siv3D (Mac): ${SIV3D_MAC_ROOT}")
message(STATUS "  Include: ${SIV3D_MAC_INCLUDE_DIR}")
message(STATUS "  Library: ${SIV3D_MAC_LIB_DIR}")

