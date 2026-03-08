# Franked License Demo — C++

A minimal C++ application that checks a Franked license before running. Build it to an `.exe` (Windows) or binary (Linux/macOS).

## Quick Start

1. Edit `main.cpp` — set `APP_SECRET` from the [Franked dashboard](https://franked.xyz). (License key is optional; you’ll be prompted at runtime if not set.)
2. Build (see below).
3. Run — the app checks the license; if valid, it continues.

## Build (Windows)

### Option A: Visual Studio + vcpkg

```powershell
# Install vcpkg: https://vcpkg.io
vcpkg install openssl:x64-windows

# Build
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[path-to-vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release

# Output: build\Release\license_demo.exe
```

### Option B: MSVC from command line

```powershell
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

(Requires OpenSSL. Download prebuilt from https://slproweb.com/products/Win32OpenSSL.html and set `OPENSSL_ROOT_DIR`.)

### Option C: MinGW-w64

```bash
# Install: OpenSSL, or use msys2: pacman -S mingw-w64-x86_64-openssl
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/msys64/mingw64"
cmake --build .
```

## Build (Linux)

```bash
# Ubuntu/Debian
sudo apt install libssl-dev libcurl4-openssl-dev build-essential cmake

mkdir build && cd build
cmake ..
make
./license_demo
```

## Build (macOS)

```bash
brew install openssl cmake
mkdir build && cd build
cmake .. -DOPENSSL_ROOT_DIR=$(brew --prefix openssl)
make
./license_demo
```

## Configuration

In `main.cpp`:

| Variable    | Description                                      |
|------------|--------------------------------------------------|
| `API_URL`   | Franked API URL (default works for hosted)      |
| `APP_SECRET`| 64-char hex from project → App Secret (only this needed to set up) |
| `LICENSE_KEY`| Optional. Set a key here, or leave empty to be prompted at runtime |

## Integrating Into Your App

1. Copy `license_checker.cpp` and `license_checker.h` into your project.
2. In your `main()` or startup code:

```cpp
#include "license_checker.h"

int main() {
    Franked::LicenseChecker checker(
        "https://lpapi-production.up.railway.app",
        "YOUR_APP_SECRET"
    );
    if (!checker.activate("USER_LICENSE_KEY")) {
        // Show error, exit
        return 1;
    }
    // Your app runs here
    runYourGame();
    return 0;
}
```

3. Link OpenSSL (and libcurl on Linux/macOS) when building.
