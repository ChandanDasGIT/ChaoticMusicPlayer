#include "backend/network/httpclient/httpclient.h"

#include <curl/curl.h>

#include <algorithm>
#include <memory>
#include <mutex>

namespace {

void ensureInitialized()
{
    static std::once_flag once;
    std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

size_t appendBody(char *data, size_t size, size_t count, void *userData)
{
    static_cast<std::string *>(userData)->append(data, size * count);
    return size * count;
}

HttpResponse perform(const std::string &url, const std::string *postBody,
                     const std::vector<std::string> &headers, long timeoutMs)
{
    ensureInitialized();
    HttpResponse response;

    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), &curl_easy_cleanup);
    if (!curl) {
        response.error = "Cannot initialize libcurl";
        return response;
    }

    curl_slist *headerList = nullptr;
    for (const std::string &header : headers)
        headerList = curl_slist_append(headerList, header.c_str());
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headerGuard(headerList, &curl_slist_free_all);

    CURL *handle = curl.get();
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_USERAGENT, HttpClient::userAgent());
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, ""); // any compression curl supports
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, timeoutMs);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, std::min(timeoutMs, 5000L));
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L); // required for use from threads
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, &appendBody);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response.body);
    if (headerList)
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headerList);
    if (postBody) {
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, postBody->c_str());
        curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, long(postBody->size()));
    }

    const CURLcode code = curl_easy_perform(handle);
    if (code != CURLE_OK) {
        response.error = curl_easy_strerror(code);
        return response;
    }
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response.status);
    return response;
}

} // namespace

HttpResponse HttpClient::get(const std::string &url, const std::vector<std::string> &headers,
                             long timeoutMs)
{
    return perform(url, nullptr, headers, timeoutMs);
}

HttpResponse HttpClient::post(const std::string &url, const std::string &body,
                              const std::vector<std::string> &headers, long timeoutMs)
{
    return perform(url, &body, headers, timeoutMs);
}

std::string HttpClient::urlEncode(const std::string &value)
{
    static const char *Hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size() * 3);
    for (unsigned char c : value) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
            || c == '-' || c == '_' || c == '.' || c == '~') {
            out += char(c);
        } else {
            out += '%';
            out += Hex[c >> 4];
            out += Hex[c & 0x0F];
        }
    }
    return out;
}

const char *HttpClient::userAgent()
{
    return "ChaoticMusicPlayer/0.1 (personal music player for Linux)";
}
