#pragma once

#include <string>
#include <vector>

struct HttpResponse
{
    long status = 0;    // HTTP status code; 0 if the request never completed
    std::string body;
    std::string error;  // transport error (DNS, timeout, TLS, ...)

    bool ok() const { return error.empty() && status >= 200 && status < 300; }
};

// Minimal blocking HTTP client (libcurl). Safe to call from any thread;
// call it from worker threads only, never from the UI thread.
class HttpClient
{
public:
    static HttpResponse get(const std::string &url,
                            const std::vector<std::string> &headers = {},
                            long timeoutMs = 10000);
    static HttpResponse post(const std::string &url, const std::string &body,
                             const std::vector<std::string> &headers = {},
                             long timeoutMs = 10000);

    // Percent-encodes a query parameter value.
    static std::string urlEncode(const std::string &value);

    static const char *userAgent();
};
