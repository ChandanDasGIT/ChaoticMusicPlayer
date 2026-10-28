#pragma once

#include <filesystem>
#include <map>
#include <string>

// Simple key=value settings file.
class Settings
{
public:
    explicit Settings(std::filesystem::path file);

    void load();
    bool save() const;

    std::string value(const std::string &key, const std::string &fallback = {}) const;
    int intValue(const std::string &key, int fallback) const;
    double doubleValue(const std::string &key, double fallback) const;
    bool boolValue(const std::string &key, bool fallback) const;

    void setValue(const std::string &key, const std::string &value);
    void setInt(const std::string &key, int value);
    void setDouble(const std::string &key, double value);
    void setBool(const std::string &key, bool value);

private:
    std::filesystem::path m_file;
    std::map<std::string, std::string> m_values;
};
