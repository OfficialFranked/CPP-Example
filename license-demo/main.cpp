/**
 * Franked License Demo — C++ Application with License Check
 *
 * Edit APP_SECRET and LICENSE_KEY below, then build.
 * On startup, the app checks the license before running.
 */

#include <iostream>
#include <string>
#include <cstdlib>

#include "license_checker.h"

// ═══════════════════════════════════════════════════════════════════════════
// CONFIGURATION — Replace with your values from the Franked dashboard
// ═══════════════════════════════════════════════════════════════════════════

const char* API_URL    = "https://lpapi-production.up.railway.app";
const char* APP_SECRET = "YOUR_64_CHAR_APP_SECRET";  // From project → App Secret (only this needed to set up)
const char* LICENSE_KEY = "";  // Set a key here, or leave empty to be prompted at runtime

// ═══════════════════════════════════════════════════════════════════════════
// Your application logic (runs only after license check passes)
// ═══════════════════════════════════════════════════════════════════════════

static void runYourApp() {
    std::cout << "\n";
    std::cout << "  *** License valid! Your application is running. ***\n";
    std::cout << "\n";
    std::cout << "  (Replace runYourApp() with your real app code: game loop,\n";
    std::cout << "   GUI, server, etc.)\n";
    std::cout << "\n";
}

// ═══════════════════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "Franked License Demo\n";

    std::string key(LICENSE_KEY);
    if (key.empty()) {
        std::cout << "Enter license key: ";
        std::getline(std::cin, key);
        if (key.empty()) {
            std::cerr << "No license key provided.\n";
            return 1;
        }
    }

    std::cout << "Checking license...\n";
    Franked::LicenseChecker checker(API_URL, APP_SECRET);
    bool ok = checker.activate(key);

    if (!ok) {
        std::cerr << "License check failed: " << checker.lastError() << "\n";
#ifdef _WIN32
        system("pause");
#endif
        return 1;
    }

    std::cout << "License valid.\n";
    runYourApp();

#ifdef _WIN32
    std::cout << "Press Enter to exit...\n";
    std::cin.get();
#endif
    return 0;
}
