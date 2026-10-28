#pragma once

#include <charconv>
#include <string>
#include <vector>

// Helpers for the app's tab-separated cache files (one record per line).
namespace tsv {

// Fields may not contain raw tabs or newlines.
inline std::string escape(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '\t': out += "\\t"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        default: out += c;
        }
    }
    return out;
}

inline std::string unescape(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '\\' || i + 1 == s.size()) {
            out += s[i];
            continue;
        }
        switch (s[++i]) {
        case 't': out += '\t'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        default: out += s[i];
        }
    }
    return out;
}

inline std::vector<std::string> split(const std::string &line)
{
    std::vector<std::string> fields;
    size_t start = 0;
    for (;;) {
        const size_t tab = line.find('\t', start);
        fields.push_back(line.substr(start, tab - start));
        if (tab == std::string::npos)
            return fields;
        start = tab + 1;
    }
}

template<typename T>
T toNumber(const std::string &s)
{
    T value{};
    std::from_chars(s.data(), s.data() + s.size(), value);
    return value;
}

} // namespace tsv
