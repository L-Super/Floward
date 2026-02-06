configure_file(version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version.h @ONLY)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Gui Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Gui Widgets REQUIRED)

if (ENABLE_SYNC_FEATURE)
    find_package(QT NAMES Qt6 Qt5 COMPONENTS Network WebSockets REQUIRED)
    find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Network WebSockets REQUIRED)
endif ()

get_target_property(QmakeExecutable Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
get_filename_component(QtBinDir "${QmakeExecutable}" DIRECTORY)
find_program(DEPLOY_QT_EXECUTABLE windeployqt macdeployqt HINTS "${QtBinDir}")

set(SRC_FILES
        main.cpp
        Clipboard.cpp Clipboard.h
        Item.cpp Item.h Item.ui
        AboutDialog.cpp AboutDialog.h AboutDialog.ui
        SettingDialog.cpp SettingDialog.h SettingDialog.ui
        CustomToolTip.cpp CustomToolTip.h CustomToolTip.ui
)
set(RES_FILES Clipboard.qrc)

set(APP_DESCRIPTION "Floward")
set(APP_COPYRIGHT "Copyright 2025-2077 The Floward Steam.")

if (WIN32)
    # set Windows rc info
    set(PRODUCT_ICON "icon.ico")
    set(FILE_VERSION ${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},${PROJECT_VERSION_TWEAK}) # 文件版本
    set(FILE_VERSION_STR "${PROJECT_VERSION}")
    set(PRODUCT_VERSION ${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},${PROJECT_VERSION_TWEAK}) # 产品版本
    set(PRODUCT_VERSION_STR "${PROJECT_VERSION}")
    set(COMPANY_NAME "Floward")
    set(FILE_DESCRIPTION ${APP_DESCRIPTION}) # 文件说明
    set(LEGAL_COPYRIGHT ${APP_COPYRIGHT})
    set(PRODUCT_NAME "Floward")

    set(WIN_RES_PATH ${CMAKE_CURRENT_SOURCE_DIR}/resources/win)
    configure_file(${WIN_RES_PATH}/app.rc.in ${WIN_RES_PATH}/app.rc @ONLY)

    set(RES_FILES ${RES_FILES} ${WIN_RES_PATH}/app.rc)

elseif (APPLE)
    set(MAC_RES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/mac")
    # It needs to be added to add_executable to display the bundle icon on macOS
    set(RES_FILES ${RES_FILES} ${MAC_RES_PATH}/icon.icns)
elseif (UNIX AND NOT APPLE)
    set(LINUX_RES_PATH ${CMAKE_CURRENT_SOURCE_DIR}/resources/linux)
    configure_file(${LINUX_RES_PATH}/Floward.desktop.in ${LINUX_RES_PATH}/Floward.desktop @ONLY)
endif ()

add_executable(${PROJECT_NAME}
        ${SRC_FILES}
        ${RES_FILES}
)

if (ENABLE_SYNC_FEATURE)
    add_subdirectory(net)
endif ()
add_subdirectory(utils)

if (WIN32)
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    endif ()
elseif (APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE TRUE)

    # Add icon to Mac bundle Resources dir
    set_source_files_properties(${MAC_RES_PATH}/icon.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
            MACOSX_BUNDLE_ICON_FILE "icon.icns"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.Floward.Floward"
            MACOSX_BUNDLE_INFO_STRING ${APP_DESCRIPTION}
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
            MACOSX_BUNDLE_COPYRIGHT ${APP_COPYRIGHT}
            MACOSX_BUNDLE_INFO_PLIST ${MAC_RES_PATH}/Info.plist.in)
elseif (UNIX AND NOT APPLE)
    set_target_properties(${PROJECT_NAME}
            PROPERTIES
            INSTALL_RPATH "$ORIGIN:$ORIGIN.:$ORIGIN/lib"
            BUILD_WITH_INSTALL_RPATH TRUE
    )
endif ()

# for SingleApplication
set(QAPPLICATION_CLASS QApplication CACHE STRING "Inheritance class for SingleApplication")

include(FetchContent)
FetchContent_Declare(QHotkey
        GIT_REPOSITORY https://github.com/Skycoder42/QHotkey
)
FetchContent_Declare(SingleApplication
        URL https://github.com/itay-grudev/SingleApplication/archive/refs/tags/v3.5.4.tar.gz
)
FetchContent_Declare(magic_enum
        GIT_REPOSITORY https://github.com/Neargye/magic_enum
        GIT_TAG v0.9.7
        GIT_SHALLOW ON
        GIT_PROGRESS ON
)
FetchContent_Declare(json
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog
        GIT_TAG v1.16.0
        GIT_SHALLOW ON
        GIT_PROGRESS ON
)

FetchContent_MakeAvailable(QHotkey SingleApplication magic_enum json spdlog)

target_link_libraries(${PROJECT_NAME}
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        QHotkey::QHotkey
        SingleApplication::SingleApplication
        magic_enum::magic_enum
        nlohmann_json::nlohmann_json
        spdlog::spdlog_header_only
)

if (ENABLE_SYNC_FEATURE)
    target_link_libraries(${PROJECT_NAME}
            Qt${QT_VERSION_MAJOR}::Network
            Qt${QT_VERSION_MAJOR}::WebSockets
    )
endif ()

# Deploy the Qt Application
if (WIN32)
    add_custom_command(TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMAND ${DEPLOY_QT_EXECUTABLE}
            --no-translations
            # --no-compiler-runtime
            $<TARGET_FILE:${PROJECT_NAME}>
    )
elseif (APPLE)
    add_custom_command(TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMAND ${DEPLOY_QT_EXECUTABLE}
            "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>"
            # -dmg
    )
endif ()