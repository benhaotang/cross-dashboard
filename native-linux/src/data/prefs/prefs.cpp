#include "prefs.h"

#include <glib.h>
#include <libsecret/secret.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>

namespace cd {

namespace {

struct KfFree {
    void operator()(GKeyFile* p) const
    {
        if (p) g_key_file_free(p);
    }
};
using KeyFileGuard = std::unique_ptr<GKeyFile, KfFree>;

constexpr char const* kIniGroup = "crossdashboard";

SecretSchema const* credential_schema_singleton()
{
    static SecretSchema* s = secret_schema_new("org.crossdashboard.credential.v1", SECRET_SCHEMA_NONE,
        "key", SECRET_SCHEMA_ATTRIBUTE_STRING, nullptr);
    return reinterpret_cast<SecretSchema const*>(s);
}

bool load_ini(GKeyFile* kf, std::string const& path)
{
    GError* err{};
    gboolean ok = g_key_file_load_from_file(kf, path.c_str(), G_KEY_FILE_NONE, &err);
    if (!ok) {
        if (err) g_error_free(err);
        return false;
    }
    return true;
}

bool save_ini(GKeyFile* kf, std::string const& path)
{
    GError* err{};
    gboolean ok = g_key_file_save_to_file(kf, path.c_str(), &err);
    if (!ok && err) {
        g_error_free(err);
        return false;
    }
    return static_cast<bool>(ok);
}

void trim_inplace(std::string& s)
{
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
}

std::vector<std::string> split_csv_screens(std::string const& csv)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : csv) {
        if (c == ',') {
            trim_inplace(cur);
            if (!cur.empty()) out.push_back(std::move(cur));
            cur.clear();
        }
        else cur += c;
    }
    trim_inplace(cur);
    if (!cur.empty()) out.push_back(std::move(cur));
    return out;
}

} // namespace

std::optional<std::string> SecretStore::get(std::string const& key)
{
    GError* err{};
    gchar* pw =
        secret_password_lookup_sync(credential_schema_singleton(), nullptr, &err, "key", key.c_str(), nullptr);
    if (!pw) {
        if (err) g_error_free(err);
        return std::nullopt;
    }
    std::string out{pw};
    secret_password_free(pw);
    return out;
}

bool SecretStore::set(std::string const& key, std::string const& value)
{
    GError* err{};
    gchar* label = g_strdup_printf("Cross-Dashboard credential: %s", key.c_str());
    gboolean ok =
        secret_password_store_sync(credential_schema_singleton(), SECRET_COLLECTION_DEFAULT,
            label, value.c_str(), nullptr, &err, "key", key.c_str(), nullptr);
    g_free(label);
    if (!ok) {
        if (err) g_error_free(err);
        return false;
    }
    return true;
}

bool SecretStore::remove(std::string const& key)
{
    GError* err{};
    gboolean ok = secret_password_clear_sync(credential_schema_singleton(), nullptr, &err, "key", key.c_str(),
        nullptr);
    if (!ok && err) {
        g_error_free(err);
        return false;
    }
    return static_cast<bool>(ok);
}

AppPreferences::AppPreferences()
{
    std::filesystem::path dir = g_get_user_config_dir();
    dir /= "crossdashboard";
    std::filesystem::create_directories(dir);
    ini_path_ = (dir / "prefs.ini").string();
}

std::optional<std::string> AppPreferences::theme() const
{
    KeyFileGuard kf(g_key_file_new());
    if (!load_ini(kf.get(), ini_path_)) return std::nullopt;

    GError* err{};
    gchar* v = g_key_file_get_string(kf.get(), kIniGroup, "theme", &err);
    if (!v) {
        if (err) g_error_free(err);
        return std::nullopt;
    }
    std::string out{v};
    g_free(v);
    return out;
}

std::optional<std::string> AppPreferences::visible_screens_ordered() const
{
    KeyFileGuard kf(g_key_file_new());
    if (!load_ini(kf.get(), ini_path_)) return std::nullopt;
    GError* err{};
    gchar* v = g_key_file_get_string(kf.get(), kIniGroup, "visible_screens", &err);
    if (!v) {
        if (err) g_error_free(err);
        return std::nullopt;
    }
    std::string out{v};
    g_free(v);
    return out;
}

std::optional<int> AppPreferences::sync_interval_minutes() const
{
    KeyFileGuard kf(g_key_file_new());
    if (!load_ini(kf.get(), ini_path_)) return std::nullopt;
    GError* err{};
    int v = static_cast<int>(
        g_key_file_get_integer(kf.get(), kIniGroup, "sync_interval_minutes", &err));

    if (err) {
        g_error_free(err);
        return std::nullopt;
    }
    return v;
}

std::optional<int> AppPreferences::pomodoro_work_minutes() const
{
    KeyFileGuard kf(g_key_file_new());
    if (!load_ini(kf.get(), ini_path_)) return std::nullopt;
    GError* err{};
    int v =
        static_cast<int>(g_key_file_get_integer(kf.get(), kIniGroup, "pomodoro_work_minutes", &err));
    if (err) {
        g_error_free(err);
        return std::nullopt;
    }
    return v;
}

std::optional<int> AppPreferences::pomodoro_break_minutes() const
{
    KeyFileGuard kf(g_key_file_new());
    if (!load_ini(kf.get(), ini_path_)) return std::nullopt;
    GError* err{};
    int v =
        static_cast<int>(g_key_file_get_integer(kf.get(), kIniGroup, "pomodoro_break_minutes", &err));
    if (err) {
        g_error_free(err);
        return std::nullopt;
    }
    return v;
}

std::optional<bool> AppPreferences::notifications_enabled() const
{
    KeyFileGuard kf(g_key_file_new());
    if (!load_ini(kf.get(), ini_path_)) return std::nullopt;
    GError* err{};
    gboolean v = g_key_file_get_boolean(kf.get(), kIniGroup, "notifications_enabled", &err);
    if (err) {
        g_error_free(err);
        return std::nullopt;
    }
    return static_cast<bool>(v);
}

std::optional<std::string> AppPreferences::kanban_columns_json() const
{
    KeyFileGuard kf(g_key_file_new());
    if (!load_ini(kf.get(), ini_path_)) return std::nullopt;
    GError* err{};
    gchar* v = g_key_file_get_string(kf.get(), kIniGroup, "kanban_columns_json", &err);
    if (!v) {
        if (err) g_error_free(err);
        return std::nullopt;
    }
    std::string out{v};
    g_free(v);
    return out;
}

std::optional<std::string> AppPreferences::kanban_column_tags_csv() const
{
    KeyFileGuard kf(g_key_file_new());
    if (!load_ini(kf.get(), ini_path_)) return std::nullopt;
    GError* err{};
    gchar* v = g_key_file_get_string(kf.get(), kIniGroup, "kanban_column_tags", &err);
    if (!v) {
        if (err) g_error_free(err);
        return std::nullopt;
    }
    std::string out{v};
    g_free(v);
    return out;
}

bool AppPreferences::set_theme(std::string const& v)
{
    KeyFileGuard kf(g_key_file_new());
    if (!load_ini(kf.get(), ini_path_)) {
        // new file
    }
    g_key_file_set_string(kf.get(), kIniGroup, "theme", v.c_str());
    return save_ini(kf.get(), ini_path_);
}

bool AppPreferences::set_visible_screens_ordered(std::string const& v)
{
    KeyFileGuard kf(g_key_file_new());
    if (!load_ini(kf.get(), ini_path_)) {}
    g_key_file_set_string(kf.get(), kIniGroup, "visible_screens", v.c_str());
    return save_ini(kf.get(), ini_path_);
}

bool AppPreferences::set_sync_interval_minutes(int v)
{
    KeyFileGuard kf(g_key_file_new());
    if (!load_ini(kf.get(), ini_path_)) {}
    g_key_file_set_integer(kf.get(), kIniGroup, "sync_interval_minutes", v);
    return save_ini(kf.get(), ini_path_);
}

bool AppPreferences::set_pomodoro_work_minutes(int v)
{
    KeyFileGuard kf(g_key_file_new());
    if (!load_ini(kf.get(), ini_path_)) {}
    g_key_file_set_integer(kf.get(), kIniGroup, "pomodoro_work_minutes", v);
    return save_ini(kf.get(), ini_path_);
}

bool AppPreferences::set_pomodoro_break_minutes(int v)
{
    KeyFileGuard kf(g_key_file_new());
    if (!load_ini(kf.get(), ini_path_)) {}
    g_key_file_set_integer(kf.get(), kIniGroup, "pomodoro_break_minutes", v);
    return save_ini(kf.get(), ini_path_);
}

bool AppPreferences::set_notifications_enabled(bool en)
{
    KeyFileGuard kf(g_key_file_new());
    if (!load_ini(kf.get(), ini_path_)) {}
    g_key_file_set_boolean(kf.get(), kIniGroup, "notifications_enabled", en ? TRUE : FALSE);
    return save_ini(kf.get(), ini_path_);
}

bool AppPreferences::set_kanban_columns_json(std::string const& v)
{
    KeyFileGuard kf(g_key_file_new());
    if (!load_ini(kf.get(), ini_path_)) {}
    g_key_file_set_string(kf.get(), kIniGroup, "kanban_columns_json", v.c_str());
    return save_ini(kf.get(), ini_path_);
}

bool AppPreferences::set_kanban_column_tags_csv(std::string const& v)
{
    KeyFileGuard kf(g_key_file_new());
    if (!load_ini(kf.get(), ini_path_)) {}
    g_key_file_set_string(kf.get(), kIniGroup, "kanban_column_tags", v.c_str());
    return save_ini(kf.get(), ini_path_);
}

AppSettings merged_app_preferences(AppPreferences const& prefs)
{
    AppSettings s;
    if (auto t = prefs.theme()) {
        if (*t == "light") s.theme = ThemePreference::Light;
        else if (*t == "dark") s.theme = ThemePreference::Dark;
        else s.theme = ThemePreference::System;
    }
    if (auto csv = prefs.visible_screens_ordered()) {
        auto parsed = split_csv_screens(*csv);
        std::vector<std::string> ordered;
        for (std::string& name : parsed) {
            if (name == "Settings")
                continue;
            if (!is_primary_screen_name(name))
                continue;
            if (std::find(ordered.begin(), ordered.end(), name) != ordered.end())
                continue;
            ordered.push_back(std::move(name));
        }
        if (!ordered.empty()) {
            ordered.push_back("Settings");
            s.visible_screens = std::move(ordered);
        }
    }
    if (auto m = prefs.sync_interval_minutes())
        s.widget_sync_interval_minutes = *m;

    if (auto w = prefs.pomodoro_work_minutes())
        s.pomodoro_settings.work_minutes = *w;
    if (auto b = prefs.pomodoro_break_minutes())
        s.pomodoro_settings.short_break_minutes = *b;

    if (auto n = prefs.notifications_enabled()) s.notifications_enabled = *n;

    if (auto kcsv = prefs.kanban_column_tags_csv()) {
        auto tags = split_csv_screens(*kcsv);
        std::vector<std::string> cleaned;
        cleaned.reserve(tags.size());
        for (auto& t : tags) {
            trim_inplace(t);
            for (char& ch : t)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (!t.empty()) cleaned.push_back(std::move(t));
        }
        if (!cleaned.empty()) s.kanban_columns = std::move(cleaned);
    }

    return s;
}

} // namespace cd
