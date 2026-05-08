#include "repo_utils.h"

#include <cctype>
#include <nlohmann/json.hpp>

namespace cd {

namespace {

void trim_inplace(std::string& s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
}

} // namespace

std::vector<std::string> calendars_from_selected_json(std::optional<std::string> raw_json)
{
    if (!raw_json.has_value() || raw_json->empty()) return {};
    auto arr = nlohmann::json::parse(*raw_json, nullptr, false);
    std::vector<std::string> out;
    if (arr.is_array()) {
        for (auto const& el : arr) {
            if (el.is_object() && el.contains("href") && el["href"].is_string())
                out.push_back(el["href"].get<std::string>());
            else if (el.is_string())
                out.push_back(el.get<std::string>());
        }
    }
    return out;
}

std::vector<std::string> repos_from_cred_string(std::optional<std::string> raw)
{
    if (!raw.has_value() || raw->empty()) return {};
    std::string trimmed = *raw;
    trim_inplace(trimmed);
    if (trimmed.empty()) return {};

    auto j = nlohmann::json::parse(trimmed, nullptr, false);
    if (j.is_array()) {
        std::vector<std::string> o;
        for (auto const& el : j) {
            if (el.is_string()) o.push_back(el.get<std::string>());
        }
        return o;
    }

    std::vector<std::string> split;
    size_t pos = 0;
    while (pos < trimmed.size()) {
        size_t c = trimmed.find(',', pos);
        if (c == std::string::npos) {
            std::string p = trimmed.substr(pos);
            trim_inplace(p);
            if (!p.empty()) split.push_back(p);
            break;
        }
        std::string p = trimmed.substr(pos, c - pos);
        trim_inplace(p);
        if (!p.empty()) split.push_back(p);
        pos = c + 1;
    }
    return split;
}

} // namespace cd
