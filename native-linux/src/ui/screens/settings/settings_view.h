#pragma once

#include "domain/models.h"

#include <functional>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/entry.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/notebook.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/spinbutton.h>

#include <string>
#include <utility>
#include <vector>

namespace cd {

class AppContainer;
struct NcLoginCredentials;
class SyncScheduler;

/** Preferences-style form; persists via `AppPreferences` / `SecretStore` where wired. */
class SettingsView final : public Gtk::Box {
public:
    using ThemeApplyFn = std::function<void()>;
    using NavChangedFn = std::function<void()>;

    SettingsView(AppContainer&, SyncScheduler&, ThemeApplyFn on_theme_applied, NavChangedFn on_nav_changed);

private:
    void save_theme();
    void save_sync_interval();
    void save_notifications();
    void save_pomodoro();

    void reload_nav_model_from_prefs();
    void recompute_hidden();
    void rebuild_nav_editor();
    void persist_nav_order();

    void move_visible_up(std::size_t i);
    void move_visible_down(std::size_t i);
    void hide_screen_at(std::size_t i);
    void restore_hidden(std::string const& name);

    void apply_nc_creds(NcLoginCredentials const& creds);
    void on_nextcloud_login_clicked();
    void on_discover_calendars_clicked();
    void on_save_calendar_selection_clicked();
    void save_kanban_columns_csv();
    void populate_calendar_checks(std::vector<CalDavCalendar> cals);

    AppContainer& app_;
    SyncScheduler& sync_;
    ThemeApplyFn on_theme_;
    NavChangedFn on_nav_;

    Gtk::Box nav_outer_{Gtk::ORIENTATION_VERTICAL, 8};
    Gtk::Box nav_visible_rows_{Gtk::ORIENTATION_VERTICAL, 4};
    Gtk::Box nav_hidden_rows_{Gtk::ORIENTATION_VERTICAL, 4};
    std::vector<std::string> nav_visible_;
    std::vector<std::string> nav_hidden_;

    Gtk::RadioButton::Group theme_rb_group_;
    Gtk::RadioButton theme_system_;
    Gtk::RadioButton theme_light_;
    Gtk::RadioButton theme_dark_;
    Gtk::SpinButton sync_minutes_;
    Gtk::CheckButton notifications_;
    Gtk::SpinButton pom_work_;
    Gtk::SpinButton pom_break_;

    Gtk::Entry caldav_server_;
    Gtk::Entry caldav_user_;
    Gtk::Entry caldav_password_;
    Gtk::Entry gitea_host_;
    Gtk::Entry gitea_token_;
    Gtk::Entry gitea_repos_;
    Gtk::Entry memos_host_;
    Gtk::Entry memos_token_;

    Gtk::Entry nc_server_entry_;
    Gtk::Button nc_login_btn_{"Nextcloud browser login…"};
    Gtk::Button cal_discover_btn_{"Discover CalDAV calendars"};
    Gtk::Button cal_save_selection_btn_{"Save calendar selection"};
    Gtk::ScrolledWindow cal_scroll_{};
    Gtk::Box cal_checks_box_{Gtk::ORIENTATION_VERTICAL, 4};
    std::vector<std::pair<Gtk::CheckButton*, std::string>> cal_row_hrefs_;

    Gtk::Entry kanban_columns_csv_;
    Gtk::Button save_kanban_btn_{"Save Kanban column tags"};

    Gtk::Box top_bar_{Gtk::ORIENTATION_HORIZONTAL, 8};
    Gtk::Button refresh_btn_{};
    Gtk::Notebook tabs_{};
};

} // namespace cd
