#include "license_checker.h"

#include <chrono>
#include <map>
#include <vector>
#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>

#include <openssl/hmac.h>
#include <openssl/sha.h>

#ifdef _WIN32
#  include <windows.h>
#  include <winhttp.h>
#  pragma comment(lib, "winhttp.lib")
#else
#  include <curl/curl.h>
#endif

namespace Franked {

// ── Helpers ───────────────────────────────────────────────────────────────

static std::string bytesToHex(const unsigned char* data, size_t len) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i)
        ss << std::setw(2) << static_cast<int>(data[i]);
    return ss.str();
}

static std::string randomHex(size_t bytes) {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);
    std::vector<unsigned char> buf(bytes);
    for (auto& b : buf) b = static_cast<unsigned char>(dist(gen));
    return bytesToHex(buf.data(), buf.size());
}

static std::string hmacSha256(const std::string& key, const std::string& msg) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(msg.data()), msg.size(),
         digest, &len);
    return bytesToHex(digest, len);
}

static std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

static std::string jsonGetString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos);
    if (pos == std::string::npos) return {};
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return {};
    auto end = json.find('"', pos + 1);
    if (end == std::string::npos) return {};
    return json.substr(pos + 1, end - pos - 1);
}

static std::string jsonGetNestedString(const std::string& json,
                                        const std::string& outer,
                                        const std::string& inner) {
    auto op = json.find("\"" + outer + "\"");
    if (op == std::string::npos) return {};
    op = json.find('{', op);
    if (op == std::string::npos) return {};
    auto cp = json.find('}', op);
    if (cp == std::string::npos) return {};
    return jsonGetString(json.substr(op, cp - op + 1), inner);
}

// ── HWID ──────────────────────────────────────────────────────────────────

static std::string getHwid() {
    unsigned char hash[SHA256_DIGEST_LENGTH];
#ifdef _WIN32
    DWORD serial = 0;
    if (GetVolumeInformationA("C:\\", nullptr, 0, &serial, nullptr, nullptr, nullptr, 0)) {
        std::string raw = std::to_string(serial);
        SHA256(reinterpret_cast<const unsigned char*>(raw.data()), raw.size(), hash);
        return bytesToHex(hash, SHA256_DIGEST_LENGTH);
    }
#else
    const char* paths[] = {"/etc/machine-id", "/var/lib/dbus/machine-id"};
    for (auto path : paths) {
        FILE* f = fopen(path, "r");
        if (!f) continue;
        char buf[256]{};
        if (fgets(buf, sizeof(buf), f)) {
            fclose(f);
            size_t len = strlen(buf);
            while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) --len;
            buf[len] = '\0';
            SHA256(reinterpret_cast<const unsigned char*>(buf), len, hash);
            return bytesToHex(hash, SHA256_DIGEST_LENGTH);
        }
        fclose(f);
    }
#endif
    return "0000000000000000000000000000000000000000000000000000000000000000";
}

// ── HTTP (WinHTTP on Windows, libcurl elsewhere) ───────────────────────────

struct HttpResult {
    bool ok = false;
    int status = 0;
    std::string body;
};

#ifdef _WIN32

static HttpResult httpPost(const std::string& url,
                           const std::string& body,
                           const std::map<std::string, std::string>& headers) {
    HttpResult r;
    std::wstring wurl(url.begin(), url.end());

    URL_COMPONENTSW uc{}; uc.dwStructSize = sizeof(uc);
    wchar_t host[256]{}, path[1024]{};
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath  = path; uc.dwUrlPathLength  = 1024;

    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) return r;

    HINTERNET hSession = WinHttpOpen(L"Franked-Demo/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!hSession) return r;

    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return r; }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return r;
    }

    if (uc.nScheme == INTERNET_SCHEME_HTTPS) {
        DWORD sec = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &sec);
    }

    for (const auto& [k, v] : headers) {
        std::wstring hdr(k.begin(), k.end());
        hdr += L": ";
        hdr += std::wstring(v.begin(), v.end());
        WinHttpAddRequestHeaders(hRequest, hdr.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return r;
    }
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return r;
    }

    DWORD status = 0, size = sizeof(status);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        nullptr, &status, &size);
    r.status = static_cast<int>(status);

    DWORD avail = 0;
    do {
        WinHttpQueryDataAvailable(hRequest, &avail);
        if (avail == 0) break;
        std::vector<char> buf(avail);
        DWORD read = 0;
        WinHttpReadData(hRequest, buf.data(), avail, &read);
        r.body.append(buf.data(), read);
    } while (true);

    r.ok = (r.status >= 200 && r.status < 300);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return r;
}

#else

static size_t curlWriteCb(void* ptr, size_t size, size_t nmemb, void* user) {
    reinterpret_cast<std::string*>(user)->append(
        static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

static HttpResult httpPost(const std::string& url,
                           const std::string& body,
                           const std::map<std::string, std::string>& headers) {
    HttpResult r;
    CURL* curl = curl_easy_init();
    if (!curl) return r;

    struct curl_slist* hlist = nullptr;
    for (const auto& [k, v] : headers)
        hlist = curl_slist_append(hlist, (k + ": " + v).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hlist);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &r.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    CURLcode rc = curl_easy_perform(curl);
    if (rc == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &r.status);
    r.ok = (rc == CURLE_OK && r.status >= 200 && r.status < 300);

    curl_slist_free_all(hlist);
    curl_easy_cleanup(curl);
    return r;
}

#endif

// ── LicenseChecker implementation ──────────────────────────────────────────

LicenseChecker::LicenseChecker(const std::string& api_url, const std::string& app_secret)
    : api_url_(api_url), app_secret_(app_secret) {
    while (!api_url_.empty() && api_url_.back() == '/')
        api_url_.pop_back();
#ifndef _WIN32
    curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
}

LicenseChecker::~LicenseChecker() {
#ifndef _WIN32
    curl_global_cleanup();
#endif
}

bool LicenseChecker::activate(const std::string& license_key) {
    last_error_.clear();

    // 1. Init — get project_id (no HMAC)
    std::string initBody = "{\"app_secret\":\"" + jsonEscape(app_secret_) + "\"}";
    HttpResult initRes = httpPost(api_url_ + "/client/init", initBody,
        {{"Content-Type", "application/json"}});

    if (!initRes.ok) {
        last_error_ = jsonGetString(initRes.body, "error");
        if (last_error_.empty())
            last_error_ = "Init failed (HTTP " + std::to_string(initRes.status) + ")";
        return false;
    }

    std::string project_id = jsonGetNestedString(initRes.body, "project", "id");
    if (project_id.empty()) {
        last_error_ = "Invalid init response: no project id";
        return false;
    }

    // 2. Activate — HMAC signed + X-Project-Id
    std::string hwid = getHwid();
    std::string actBody = "{\"license_key\":\"" + jsonEscape(license_key)
        + "\",\"hwid\":\"" + jsonEscape(hwid) + "\"}";

    auto ts = std::chrono::system_clock::now().time_since_epoch().count() / 1000000000;
    std::string timestamp = std::to_string(ts);
    std::string nonce = randomHex(32);
    std::string msg = timestamp + "." + nonce + "." + actBody;
    std::string sig = hmacSha256(app_secret_, msg);

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"X-Signature", sig},
        {"X-Timestamp", timestamp},
        {"X-Nonce", nonce},
        {"X-Project-Id", project_id},
    };

    HttpResult actRes = httpPost(api_url_ + "/client/license/activate", actBody, headers);

    if (!actRes.ok) {
        last_error_ = jsonGetString(actRes.body, "error");
        if (last_error_.empty())
            last_error_ = "Activation failed (HTTP " + std::to_string(actRes.status) + ")";
        return false;
    }

    return true;  // 2xx = success
}

} // namespace Franked
