#pragma once

#include <string>

namespace Franked {

/**
 * License checker for Franked API.
 * Call activate() at startup; if it returns false, refuse to run.
 */
class LicenseChecker {
public:
    LicenseChecker(const std::string& api_url, const std::string& app_secret);
    ~LicenseChecker();

    /** Activate a license key. Returns true if valid. */
    bool activate(const std::string& license_key);

    /** Last error message (if activate returned false). */
    const std::string& lastError() const { return last_error_; }

private:
    std::string api_url_;
    std::string app_secret_;
    std::string last_error_;
};

} // namespace Franked
