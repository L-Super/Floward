cmake_minimum_required(VERSION 3.25)
project(Floward
        VERSION 3.0.4.0 # Set application version. For more details, see src/version.h.in
)

# Adapt to the backward C++ standard support on Linux platforms.
if (UNIX AND NOT APPLE)
    set(CMAKE_CXX_STANDARD 20)
else ()
    set(CMAKE_CXX_STANDARD 23)
endif ()

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(ENABLE_SYNC_FEATURE "enable sync feature" OFF)

# --- Encryption key (symmetric AES-256-GCM) -----------------------------
# The sync version encrypts clipboard data before uploading so the server
# only ever sees ciphertext. The key is a 64-char hex string (32 bytes).
#
# Source priority:
#   1. CMAKE_CRYPTO_KEY cmake variable  (explicit -DCMAKE_CRYPTO_KEY=...)
#   2. CRYPTO_KEY environment variable  (CI: GitHub Actions secret)
#   3. Hard-coded dev default           (local builds only, NOT for releases)
#
# In CI the key is injected via the CRYPTO_KEY env var that maps to a
# GitHub Actions secret. The dev default below is public and only exists
# so local builds work without any configuration — it MUST NOT ship in a
# release build.
if (NOT DEFINED CMAKE_CRYPTO_KEY)
    if (DEFINED ENV{CRYPTO_KEY})
        set(CMAKE_CRYPTO_KEY "$ENV{CRYPTO_KEY}")
        message(STATUS "Crypto key: from CRYPTO_KEY env var")
    else ()
        set(CMAKE_CRYPTO_KEY "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f")
        message(WARNING "Crypto key: using PUBLIC dev default — do NOT ship in a release build")
    endif ()
else ()
    message(STATUS "Crypto key: from CMAKE_CRYPTO_KEY cmake variable")
endif ()

# Basic sanity check: must be 64 hex chars
string(LENGTH "${CMAKE_CRYPTO_KEY}" _crypto_key_len)
if (NOT _crypto_key_len EQUAL 64)
    message(FATAL_ERROR "CRYPTO_KEY must be 64 hex chars (32 bytes), got ${_crypto_key_len} chars: '${CMAKE_CRYPTO_KEY}'")
endif ()
string(REGEX MATCH "^[0-9a-fA-F]+$" _crypto_key_match "${CMAKE_CRYPTO_KEY}")
if (NOT _crypto_key_match)
    message(FATAL_ERROR "CRYPTO_KEY must be hex chars only: '${CMAKE_CRYPTO_KEY}'")
endif ()
unset(_crypto_key_len)
unset(_crypto_key_match)

if (ENABLE_SYNC_FEATURE)
    add_compile_definitions(ENABLE_SYNC)
    # define protocol scheme marco
    add_compile_definitions(PROTOCOL_SCHEME="floward")
    # expose the encryption key to the preprocessor as a quoted hex string
    add_compile_definitions(CRYPTO_KEY_HEX="${CMAKE_CRYPTO_KEY}")
    message(STATUS "Enable sync version")
else ()
    message(STATUS "Enable standalone version")
endif ()

if (WIN32 AND MSVC)
    add_compile_options(/utf-8)
endif ()

add_subdirectory(src)