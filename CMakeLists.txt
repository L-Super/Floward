cmake_minimum_required(VERSION 3.25)
project(Floward
        VERSION 3.0.2.0 # Set application version. For more details, see src/version.h.in
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(ENABLE_SYNC_FEATURE "enable sync feature" OFF)

if (ENABLE_SYNC_FEATURE)
    add_compile_definitions(ENABLE_SYNC)
    # define protocol scheme marco
    add_compile_definitions(PROTOCOL_SCHEME="floward")
    message(STATUS "Enable sync version")
else ()
    message(STATUS "Enable standalone version")
endif ()

if (WIN32 AND MSVC)
    add_compile_options(/utf-8)
endif ()

add_subdirectory(src)