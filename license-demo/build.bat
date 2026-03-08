@echo off
REM Build script for Windows (requires OpenSSL and Visual Studio or MinGW)
REM Option 1: Use vcpkg: vcpkg install openssl:x64-windows
REM Option 2: Download OpenSSL from https://slproweb.com/products/Win32OpenSSL.html

if not exist build mkdir build
cd build

REM Using MSVC (run from "x64 Native Tools Command Prompt")
REM Set OPENSSL_DIR to your OpenSSL install path if needed
if "%OPENSSL_ROOT_DIR%"=="" set OPENSSL_ROOT_DIR=C:\Program Files\OpenSSL-Win64

cl /nologo /EHsc /std:c++17 /O2 /I.. /I"%OPENSSL_ROOT_DIR%\include" ^
    ..\license_checker.cpp ..\main.cpp ^
    /Fe:license_demo.exe ^
    /link /libpath:"%OPENSSL_ROOT_DIR%\lib" libssl.lib libcrypto.lib winhttp.lib

if %ERRORLEVEL% equ 0 (
    echo Built: build\license_demo.exe
) else (
    echo Build failed. Try using CMake instead - see README.md
)
cd ..
