#include "backend/settings/settings/settings.h"

#include <charconv>
#include <fstream>
#include <locale>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {

std::string trimmed(const std::string &s)
{
    const auto begin = s.find_first_not_of(" \t\r");
    if (begin == std::string::npos)
        return {};
    const auto end = s.find_last_not_of(" \t\r");
    return s.substr(begin, end - begin + 1);
}

} // namespace

Settings::Settings(fs::path file)
    : m_file(std::move(file))
{
}

void Settings::load()
{
    m_values.clear();
    std::ifstream in(m_file);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line.front() == '#')
            continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        m_values[trimmed(line.substr(0, eq))] = trimmed(line.substr(eq + 1));
    }
}

bool Settings::save() const
{
    std::error_code ec;
    fs::create_directories(m_file.parent_path(), ec);

    const fs::path temp = m_file.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out)
            return false;
        for (const auto &[key, value] : m_values)
            out << key << '=' << value << '\n';
        if (!out)
            return false;
    }
    fs::rename(temp, m_file, ec);
    return !ec;
}

std::string Settings::value(const std::string &key, const std::string &fallback) const
{
    const auto it = m_values.find(key);
    return it != m_values.end() ? it->second : fallback;
}

int Settings::intValue(const std::string &key, int fallback) const
{
    const std::string s = value(key);
    int result = 0;
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), result);
    return ec == std::errc() && ptr == s.data() + s.size() && !s.empty() ? result : fallback;
}

double Settings::doubleValue(const std::string &key, double fallback) const
{
    const std::string s = value(key);
    // (A stream in the "C" locale: Android's C++ library has no from_chars for double)
    std::istringstream in(s);
    in.imbue(std::locale::classic());
    double result = 0;
    in >> result;
    return !s.empty() && !in.fail() && in.peek() == std::char_traits<char>::eof() ? result : fallback;
}

bool Settings::boolValue(const std::string &key, bool fallback) const
{
    const std::string s = value(key);
    if (s == "true" || s == "1")
        return true;
    if (s == "false" || s == "0")
        return false;
    return fallback;
}

void Settings::setValue(const std::string &key, const std::string &value)
{
    m_values[key] = value;
}

void Settings::setInt(const std::string &key, int value)
{
    setValue(key, std::to_string(value));
}

void Settings::setDouble(const std::string &key, double value)
{
    char buffer[32];
    const auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value);
    setValue(key, ec == std::errc() ? std::string(buffer, ptr) : std::to_string(value));
}

void Settings::setBool(const std::string &key, bool value)
{
    setValue(key, value ? "true" : "false");
}
