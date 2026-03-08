cmake_minimum_required(VERSION 3.16)
project(license_demo VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OpenSSL for HMAC-SHA256
find_package(OpenSSL REQUIRED)

# Windows: WinHTTP (built-in). Linux/macOS: libcurl
if(WIN32)
    # WinHTTP is part of Windows SDK
else()
    find_package(CURL REQUIRED)
endif()

add_executable(license_demo
    main.cpp
    license_checker.cpp
)

target_include_directories(license_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(license_demo PRIVATE OpenSSL::Crypto)

if(WIN32)
    target_link_libraries(license_demo PRIVATE winhttp)
else()
    target_link_libraries(license_demo PRIVATE CURL::libcurl)
endif()
