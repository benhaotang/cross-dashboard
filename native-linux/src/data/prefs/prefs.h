#pragma once

#include <optional>
#include <string>

#include "domain/models.h"

namespace cd {

/** Same string keys as Android `CredentialKey` / legacy keyring. */
namespace CredentialKey {
inline constexpr char CALDAV_SERVER[] = "caldav_server";
inline constexpr char CALDAV_USERNAME[] = "caldav_username";
inline constexpr char CALDAV_PASSWORD[] = "caldav_password";
inline constexpr char CALDAV_AUTH_METHOD[] = "caldav_auth_method";
inline constexpr char CALDAV_SELECTED_CALENDARS[] = "caldav_selected_calendars";
inline constexpr char CALDAV_DEFAULT_EVENT_CALENDAR[] = "caldav_default_event_calendar";
inline constexpr char CALDAV_DEFAULT_TASK_CALENDAR[] = "caldav_default_task_calendar";
inline constexpr char CALDAV_DEFAULT_NOTE_CALENDAR[] = "caldav_default_note_calendar";
inline constexpr char NEXTCLOUD_SSO_ACCOUNT[] = "nextcloud_sso_account";
inline constexpr char GITEA_TOKEN[] = "gitea_token";
inline constexpr char GITEA_INSTANCE[] = "gitea_instance";
inline constexpr char GITEA_REPOS[] = "gitea_repos";
inline constexpr char MEMOS_HOST[] = "memos_host";
inline constexpr char MEMOS_TOKEN[] = "memos_token";
inline constexpr char KARAKEEP_HOST[] = "karakeep_host";
inline constexpr char KARAKEEP_TOKEN[] = "karakeep_token";
} // namespace CredentialKey

/** Credential storage backed by Secret Service (`libsecret-1`). */
class SecretStore final {
public:
    [[nodiscard]] std::optional<std::string> get(std::string const& key);
    [[nodiscard]] bool set(std::string const& key, std::string const& value);
    [[nodiscard]] bool remove(std::string const& key);
};

/** Non-sensitive settings in `~/.config/crossdashboard/prefs.ini` (GLib key file). */
class AppPreferences final {
public:
    AppPreferences();

    [[nodiscard]] std::optional<std::string> theme() const; // auto / light / dark
    [[nodiscard]] std::optional<std::string> visible_screens_ordered() const;
    [[nodiscard]] std::optional<int> sync_interval_minutes() const;
    [[nodiscard]] std::optional<std::string> timezone_override() const;
    [[nodiscard]] std::optional<int> pomodoro_work_minutes() const;
    [[nodiscard]] std::optional<int> pomodoro_break_minutes() const;
    [[nodiscard]] std::optional<bool> notifications_enabled() const;
    /** JSON object: task UID string → Kanban column index 0–3 (legacy; tag-based Kanban ignores this). */
    [[nodiscard]] std::optional<std::string> kanban_columns_json() const;
    /** Comma-separated Kanban column tag names, e.g. `backlog,planned,inprogress,done` — matches Android. */
    [[nodiscard]] std::optional<std::string> kanban_column_tags_csv() const;
    [[nodiscard]] std::optional<std::string> background_command() const;
    [[nodiscard]] std::optional<std::string> background_template_json() const;
    [[nodiscard]] std::optional<std::string> background_image_path() const;
    [[nodiscard]] std::optional<std::string> background_light_image_path() const;
    [[nodiscard]] std::optional<std::string> background_dark_image_path() const;
    [[nodiscard]] std::optional<std::string> background_image_fit() const;
    [[nodiscard]] std::optional<std::string> background_glass_opacity() const;

    [[nodiscard]] bool set_theme(std::string const&);
    [[nodiscard]] bool set_visible_screens_ordered(std::string const&);
    [[nodiscard]] bool set_sync_interval_minutes(int);
    [[nodiscard]] bool set_timezone_override(std::optional<std::string> const&);
    [[nodiscard]] bool set_pomodoro_work_minutes(int);
    [[nodiscard]] bool set_pomodoro_break_minutes(int);
    [[nodiscard]] bool set_notifications_enabled(bool);
    [[nodiscard]] bool set_kanban_columns_json(std::string const&);
    [[nodiscard]] bool set_kanban_column_tags_csv(std::string const&);
    [[nodiscard]] bool set_background_command(std::string const&);
    [[nodiscard]] bool set_background_template_json(std::string const&);
    [[nodiscard]] bool set_background_image_path(std::string const&);
    [[nodiscard]] bool set_background_light_image_path(std::string const&);
    [[nodiscard]] bool set_background_dark_image_path(std::string const&);
    [[nodiscard]] bool set_background_image_fit(std::string const&);
    [[nodiscard]] bool set_background_glass_opacity(std::string const&);

private:
    std::string ini_path_;
};

/** Apply stored prefs as overrides on top of `AppSettings` defaults. */
AppSettings merged_app_preferences(AppPreferences const& prefs);

std::string const& system_timezone_id();
bool apply_timezone_override(std::optional<std::string> const& zone_id);

} // namespace cd
