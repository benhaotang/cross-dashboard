#include "settings_view.h"

#include "app_container.h"
#include "data/network/nextcloud_login_flow.h"
#include "data/prefs/prefs.h"
#include "data/repository/repo_utils.h"
#include "domain/models.h"

#include <algorithm>

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>

#include <glib.h>

#include <gtkmm/button.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/separator.h>

extern "C" {
#include <atk/atk.h>
#include <gtk/gtk.h>
}

namespace cd {

namespace {

Gtk::ScrolledWindow* make_scrolled(Gtk::Widget& child)
{
    auto* sw = Gtk::manage(new Gtk::ScrolledWindow());
    sw->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    sw->add(child);
    return sw;
}

void pack_section_title(Gtk::Box& col, char const* title)
{
    auto* l = Gtk::manage(new Gtk::Label());
    l->set_markup(std::string("<b>") + title + "</b>");
    l->set_halign(Gtk::ALIGN_START);
    l->set_margin_top(6);
    col.pack_start(*l, false, false);
}

} // namespace

#ifndef CD_APP_VERSION
#define CD_APP_VERSION "dev"
#endif

SettingsView::SettingsView(AppContainer& app, ThemeApplyFn on_theme_applied, NavChangedFn on_nav_changed)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 10)
    , app_(app)
    , on_theme_(std::move(on_theme_applied))
    , on_nav_(std::move(on_nav_changed))
    , theme_rb_group_{}
    , theme_system_{theme_rb_group_, Glib::ustring("Match system")}
    , theme_light_{theme_rb_group_, Glib::ustring("Light")}
    , theme_dark_{theme_rb_group_, Glib::ustring("Dark")}
    , sync_minutes_(Gtk::Adjustment::create(60, 5, 24 * 60, 1, 10))
    , notifications_{}
    , pom_work_(Gtk::Adjustment::create(25, 1, 120, 1, 5))
    , pom_break_(Gtk::Adjustment::create(5, 1, 60, 1, 5))
{
    set_margin_start(8);
    set_margin_end(8);
    set_margin_top(8);
    set_margin_bottom(8);

    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(tabs_.gobj())), "cd-settings-notebook");

    AppSettings const s = merged_app_preferences(app_.prefs());
    if (s.theme == ThemePreference::Light)
        theme_light_.set_active(true);
    else if (s.theme == ThemePreference::Dark)
        theme_dark_.set_active(true);
    else
        theme_system_.set_active(true);

    sync_minutes_.set_value(s.widget_sync_interval_minutes);
    notifications_.set_active(s.notifications_enabled);
    pom_work_.set_value(s.pomodoro_settings.work_minutes);
    pom_break_.set_value(s.pomodoro_settings.short_break_minutes);

    if (auto v = app_.secrets().get(CredentialKey::CALDAV_SERVER)) caldav_server_.set_text(*v);
    if (auto v = app_.secrets().get(CredentialKey::CALDAV_USERNAME)) caldav_user_.set_text(*v);
    if (auto v = app_.secrets().get(CredentialKey::CALDAV_PASSWORD)) caldav_password_.set_text(*v);
    if (auto v = app_.secrets().get(CredentialKey::GITEA_TOKEN)) gitea_token_.set_text(*v);
    if (auto v = app_.secrets().get(CredentialKey::GITEA_INSTANCE)) gitea_host_.set_text(*v);
    if (auto v = app_.secrets().get(CredentialKey::GITEA_REPOS)) gitea_repos_.set_text(*v);
    if (auto v = app_.secrets().get(CredentialKey::MEMOS_HOST)) memos_host_.set_text(*v);
    if (auto v = app_.secrets().get(CredentialKey::MEMOS_TOKEN)) memos_token_.set_text(*v);

    caldav_password_.set_visibility(false);
    gitea_token_.set_visibility(false);
    memos_token_.set_visibility(false);

    if (auto v = app_.secrets().get(CredentialKey::CALDAV_SERVER)) nc_server_entry_.set_text(*v);

    std::string kcsv;
    for (std::size_t i = 0; i < s.kanban_columns.size(); ++i) {
        if (i) kcsv += ',';
        kcsv += s.kanban_columns[i];
    }
    kanban_columns_csv_.set_text(kcsv);
    kanban_columns_csv_.set_placeholder_text("backlog,planned,inprogress,done");
    gitea_repos_.set_placeholder_text(R"(["owner/repo"] or owner/repo,owner/repo2)");

    auto page_box = [](Gtk::Orientation o = Gtk::ORIENTATION_VERTICAL) {
        auto* b = Gtk::manage(new Gtk::Box(o, 10));
        b->set_margin_start(16);
        b->set_margin_end(16);
        b->set_margin_top(16);
        b->set_margin_bottom(24);
        return b;
    };

    // CalDAV (account, Nextcloud login, calendars, Kanban) — mirrors macOS CalDAV tab scope.
    auto* caldav_page = page_box();
    pack_section_title(*caldav_page, "CalDAV account");
    auto* caldav_fields = Gtk::manage(new Gtk::Grid());
    caldav_fields->set_column_spacing(10);
    caldav_fields->set_row_spacing(8);
    int dr = 0;
    caldav_fields->attach(*Gtk::manage(new Gtk::Label("Server URL")), 0, dr, 1, 1);
    caldav_fields->attach(caldav_server_, 1, dr++, 1, 1);
    caldav_server_.set_hexpand(true);
    caldav_fields->attach(*Gtk::manage(new Gtk::Label("Username")), 0, dr, 1, 1);
    caldav_fields->attach(caldav_user_, 1, dr++, 1, 1);
    caldav_user_.set_hexpand(true);
    caldav_fields->attach(*Gtk::manage(new Gtk::Label("Password")), 0, dr, 1, 1);
    caldav_fields->attach(caldav_password_, 1, dr++, 1, 1);
    caldav_password_.set_hexpand(true);
    caldav_page->pack_start(*caldav_fields, false, false);
    auto* save_caldav = Gtk::manage(new Gtk::Button("Save CalDAV credentials"));
    save_caldav->signal_clicked().connect([this] {
        (void)app_.secrets().set(CredentialKey::CALDAV_SERVER, std::string(caldav_server_.get_text()));
        (void)app_.secrets().set(CredentialKey::CALDAV_USERNAME, std::string(caldav_user_.get_text()));
        (void)app_.secrets().set(CredentialKey::CALDAV_PASSWORD, std::string(caldav_password_.get_text()));
    });
    caldav_page->pack_start(*save_caldav, false, false);

    caldav_page->pack_start(*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)), false, false);
    pack_section_title(*caldav_page, "Nextcloud browser login");
    auto* nc_grid = Gtk::manage(new Gtk::Grid());
    nc_grid->set_column_spacing(10);
    nc_grid->set_row_spacing(8);
    nc_grid->attach(*Gtk::manage(new Gtk::Label("Server URL")), 0, 0, 1, 1);
    nc_grid->attach(nc_server_entry_, 1, 0, 1, 1);
    nc_server_entry_.set_hexpand(true);
    caldav_page->pack_start(*nc_grid, false, false);
    nc_login_btn_.signal_clicked().connect(sigc::mem_fun(*this, &SettingsView::on_nextcloud_login_clicked));
    caldav_page->pack_start(nc_login_btn_, false, false);

    pack_section_title(*caldav_page, "Calendars");
    cal_discover_btn_.signal_clicked().connect(sigc::mem_fun(*this, &SettingsView::on_discover_calendars_clicked));
    caldav_page->pack_start(cal_discover_btn_, false, false);
    cal_scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    cal_scroll_.set_min_content_height(160);
    cal_scroll_.add(cal_checks_box_);
    caldav_page->pack_start(cal_scroll_, true, true);
    cal_save_selection_btn_.signal_clicked().connect(
        sigc::mem_fun(*this, &SettingsView::on_save_calendar_selection_clicked));
    caldav_page->pack_start(cal_save_selection_btn_, false, false);

    pack_section_title(*caldav_page, "Views");
    auto* views_grid = Gtk::manage(new Gtk::Grid());
    views_grid->set_column_spacing(10);
    views_grid->set_row_spacing(8);
    views_grid->attach(*Gtk::manage(new Gtk::Label("Kanban columns (comma-separated)")), 0, 0, 1, 1);
    views_grid->attach(kanban_columns_csv_, 1, 0, 1, 1);
    kanban_columns_csv_.set_hexpand(true);
    caldav_page->pack_start(*views_grid, false, false);
    save_kanban_btn_.signal_clicked().connect(sigc::mem_fun(*this, &SettingsView::save_kanban_columns_csv));
    caldav_page->pack_start(save_kanban_btn_, false, false);

    // Gitea
    auto* gitea_page = page_box();
    pack_section_title(*gitea_page, "Gitea connection");
    auto* g_grid = Gtk::manage(new Gtk::Grid());
    g_grid->set_column_spacing(10);
    g_grid->set_row_spacing(8);
    int gr = 0;
    g_grid->attach(*Gtk::manage(new Gtk::Label("Instance URL")), 0, gr, 1, 1);
    g_grid->attach(gitea_host_, 1, gr++, 1, 1);
    gitea_host_.set_hexpand(true);
    g_grid->attach(*Gtk::manage(new Gtk::Label("Access token")), 0, gr, 1, 1);
    g_grid->attach(gitea_token_, 1, gr++, 1, 1);
    gitea_token_.set_hexpand(true);
    g_grid->attach(*Gtk::manage(new Gtk::Label("Repositories (JSON or CSV)")), 0, gr, 1, 1);
    g_grid->attach(gitea_repos_, 1, gr++, 1, 1);
    gitea_repos_.set_hexpand(true);
    gitea_page->pack_start(*g_grid, false, false);
    auto* save_gitea = Gtk::manage(new Gtk::Button("Save Gitea settings"));
    save_gitea->signal_clicked().connect([this] {
        (void)app_.secrets().set(CredentialKey::GITEA_INSTANCE, std::string(gitea_host_.get_text()));
        (void)app_.secrets().set(CredentialKey::GITEA_TOKEN, std::string(gitea_token_.get_text()));
        (void)app_.secrets().set(CredentialKey::GITEA_REPOS, std::string(gitea_repos_.get_text()));
    });
    gitea_page->pack_start(*save_gitea, false, false);

    // Capture (API host + token)
    auto* memos_page = page_box();
    pack_section_title(*memos_page, "Capture server");
    auto* m_grid = Gtk::manage(new Gtk::Grid());
    m_grid->set_column_spacing(10);
    m_grid->set_row_spacing(8);
    m_grid->attach(*Gtk::manage(new Gtk::Label("Host URL")), 0, 0, 1, 1);
    m_grid->attach(memos_host_, 1, 0, 1, 1);
    memos_host_.set_hexpand(true);
    m_grid->attach(*Gtk::manage(new Gtk::Label("API token")), 0, 1, 1, 1);
    m_grid->attach(memos_token_, 1, 1, 1, 1);
    memos_token_.set_hexpand(true);
    memos_page->pack_start(*m_grid, false, false);
    auto* save_memos = Gtk::manage(new Gtk::Button("Save Capture settings"));
    save_memos->signal_clicked().connect([this] {
        (void)app_.secrets().set(CredentialKey::MEMOS_HOST, std::string(memos_host_.get_text()));
        (void)app_.secrets().set(CredentialKey::MEMOS_TOKEN, std::string(memos_token_.get_text()));
    });
    memos_page->pack_start(*save_memos, false, false);

    // Appearance
    auto* appear_page = page_box();
    pack_section_title(*appear_page, "Theme");
    auto* tgrid = Gtk::manage(new Gtk::Grid());
    tgrid->set_column_spacing(8);
    tgrid->set_row_spacing(6);
    int tr = 0;
    tgrid->attach(theme_system_, 0, tr++, 2, 1);
    tgrid->attach(theme_light_, 0, tr++, 2, 1);
    tgrid->attach(theme_dark_, 0, tr++, 2, 1);
    appear_page->pack_start(*tgrid, false, false);
    auto* apply_theme = Gtk::manage(new Gtk::Button("Apply theme"));
    apply_theme->signal_clicked().connect(sigc::mem_fun(*this, &SettingsView::save_theme));
    appear_page->pack_start(*apply_theme, false, false);

    // General (About + sync — macOS splits notifications; we keep sync + notifications here)
    auto* general_page = page_box();
    auto* about = Gtk::manage(new Gtk::Label());
    about->set_halign(Gtk::ALIGN_START);
    about->set_markup("<b>About</b>\nCross-Dashboard Linux · version " CD_APP_VERSION);
    general_page->pack_start(*about, false, false);
    pack_section_title(*general_page, "Sync & notifications");
    auto* sgrid = Gtk::manage(new Gtk::Grid());
    sgrid->set_column_spacing(10);
    sgrid->set_row_spacing(8);
    sgrid->attach(*Gtk::manage(new Gtk::Label("Sync interval (minutes)")), 0, 0, 1, 1);
    sgrid->attach(sync_minutes_, 1, 0, 1, 1);
    notifications_.set_label("Enable notifications (background sync & reminders)");
    sgrid->attach(notifications_, 0, 1, 2, 1);
    general_page->pack_start(*sgrid, false, false);
    auto* save_general = Gtk::manage(new Gtk::Button("Save"));
    save_general->signal_clicked().connect([this] {
        save_sync_interval();
        save_notifications();
    });
    general_page->pack_start(*save_general, false, false);

    // Pomodoro
    auto* pom_page = page_box();
    pack_section_title(*pom_page, "Timer defaults");
    auto* pgrid = Gtk::manage(new Gtk::Grid());
    pgrid->set_column_spacing(10);
    pgrid->set_row_spacing(8);
    pgrid->attach(*Gtk::manage(new Gtk::Label("Work (minutes)")), 0, 0, 1, 1);
    pgrid->attach(pom_work_, 1, 0, 1, 1);
    pgrid->attach(*Gtk::manage(new Gtk::Label("Break (minutes)")), 0, 1, 1, 1);
    pgrid->attach(pom_break_, 1, 1, 1, 1);
    pom_page->pack_start(*pgrid, false, false);
    auto* save_pom_btn = Gtk::manage(new Gtk::Button("Save"));
    save_pom_btn->signal_clicked().connect(sigc::mem_fun(*this, &SettingsView::save_pomodoro));
    pom_page->pack_start(*save_pom_btn, false, false);

    // Navigation
    reload_nav_model_from_prefs();
    nav_outer_.set_margin_start(16);
    nav_outer_.set_margin_end(16);
    nav_outer_.set_margin_top(16);
    nav_outer_.set_margin_bottom(24);
    auto* intro = Gtk::manage(new Gtk::Label(
        "Reorder sidebar items (↑/↓). Hide removes a screen from the sidebar; Restore adds it back. "
        "Settings stays last."));
    intro->set_line_wrap(true);
    intro->set_halign(Gtk::ALIGN_START);
    nav_outer_.pack_start(*intro, false, false);
    nav_outer_.pack_start(nav_visible_rows_, false, false);
    auto* hidden_l = Gtk::manage(new Gtk::Label());
    hidden_l->set_markup("<b>Hidden screens</b>");
    hidden_l->set_halign(Gtk::ALIGN_START);
    nav_outer_.pack_start(*hidden_l, false, false);
    nav_outer_.pack_start(nav_hidden_rows_, false, false);
    rebuild_nav_editor();

    tabs_.append_page(*make_scrolled(*caldav_page), "CalDAV");
    tabs_.append_page(*make_scrolled(*gitea_page), "Gitea");
    tabs_.append_page(*make_scrolled(*memos_page), "Capture");
    tabs_.append_page(*make_scrolled(*appear_page), "Appearance");
    tabs_.append_page(*make_scrolled(*general_page), "General");
    tabs_.append_page(*make_scrolled(*pom_page), "Pomodoro");
    tabs_.append_page(*make_scrolled(nav_outer_), "Navigation");

    pack_start(tabs_, true, true);

    show_all_children();
}

void SettingsView::save_theme()
{
    if (theme_light_.get_active())
        (void)app_.prefs().set_theme("light");
    else if (theme_dark_.get_active())
        (void)app_.prefs().set_theme("dark");
    else
        (void)app_.prefs().set_theme("auto");

    if (on_theme_) on_theme_();
}

void SettingsView::save_sync_interval()
{
    (void)app_.prefs().set_sync_interval_minutes(static_cast<int>(sync_minutes_.get_value()));
}

void SettingsView::save_notifications()
{
    (void)app_.prefs().set_notifications_enabled(notifications_.get_active());
}

void SettingsView::save_pomodoro()
{
    (void)app_.prefs().set_pomodoro_work_minutes(static_cast<int>(pom_work_.get_value()));
    (void)app_.prefs().set_pomodoro_break_minutes(static_cast<int>(pom_break_.get_value()));
}

void SettingsView::reload_nav_model_from_prefs()
{
    AppSettings const s = merged_app_preferences(app_.prefs());
    nav_visible_.clear();
    for (auto const& n : s.visible_screens) {
        if (n == "Settings")
            continue;
        if (is_primary_screen_name(n))
            nav_visible_.push_back(n);
    }
    if (nav_visible_.empty())
        nav_visible_.assign(kAllScreens.begin(), kAllScreens.end());
    recompute_hidden();
}

void SettingsView::recompute_hidden()
{
    nav_hidden_.clear();
    for (auto* p : kAllScreens) {
        std::string const x(p);
        if (std::find(nav_visible_.begin(), nav_visible_.end(), x) == nav_visible_.end())
            nav_hidden_.push_back(x);
    }
}

void SettingsView::rebuild_nav_editor()
{
    for (auto* w : nav_visible_rows_.get_children())
        nav_visible_rows_.remove(*w);
    for (auto* w : nav_hidden_rows_.get_children())
        nav_hidden_rows_.remove(*w);

    for (std::size_t i = 0; i < nav_visible_.size(); ++i) {
        auto* row = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 6));
        auto* lab = Gtk::manage(new Gtk::Label(nav_visible_[i]));
        lab->set_halign(Gtk::ALIGN_START);
        auto* up = Gtk::manage(new Gtk::Button("\u2191"));
        auto* dn = Gtk::manage(new Gtk::Button("\u2193"));
        auto* hide = Gtk::manage(new Gtk::Button("Hide"));
        gtk_widget_set_tooltip_text(GTK_WIDGET(up->gobj()), "Move up");
        gtk_widget_set_tooltip_text(GTK_WIDGET(dn->gobj()), "Move down");
        gtk_widget_set_tooltip_text(GTK_WIDGET(hide->gobj()), "Hide from sidebar");
        if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(up->gobj())))
            atk_object_set_name(a, "Move screen up in sidebar");
        if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(dn->gobj())))
            atk_object_set_name(a, "Move screen down in sidebar");
        if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(hide->gobj())))
            atk_object_set_name(a, "Hide screen from sidebar");

        std::size_t const idx = i;
        up->set_sensitive(idx > 0);
        dn->set_sensitive(idx + 1 < nav_visible_.size());
        hide->set_sensitive(nav_visible_.size() > 1);
        up->signal_clicked().connect([this, idx] { move_visible_up(idx); });
        dn->signal_clicked().connect([this, idx] { move_visible_down(idx); });
        hide->signal_clicked().connect([this, idx] { hide_screen_at(idx); });
        row->pack_start(*lab, true, true);
        row->pack_start(*up, false, false);
        row->pack_start(*dn, false, false);
        row->pack_start(*hide, false, false);
        nav_visible_rows_.pack_start(*row, false, false);
    }

    for (std::string const& h : nav_hidden_) {
        auto* row = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 6));
        auto* lab = Gtk::manage(new Gtk::Label(h));
        lab->set_halign(Gtk::ALIGN_START);
        auto* rest = Gtk::manage(new Gtk::Button("Restore"));
        gtk_widget_set_tooltip_text(GTK_WIDGET(rest->gobj()), "Show in sidebar again");
        if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(rest->gobj())))
            atk_object_set_name(a, "Restore screen to sidebar");
        rest->signal_clicked().connect([this, h] { restore_hidden(h); });
        row->pack_start(*lab, true, true);
        row->pack_start(*rest, false, false);
        nav_hidden_rows_.pack_start(*row, false, false);
    }

    nav_visible_rows_.show_all();
    nav_hidden_rows_.show_all();
}

void SettingsView::persist_nav_order()
{
    std::string csv;
    for (std::size_t i = 0; i < nav_visible_.size(); ++i) {
        if (i) csv += ',';
        csv += nav_visible_[i];
    }
    csv += ",Settings";
    (void)app_.prefs().set_visible_screens_ordered(csv);
    if (on_nav_) on_nav_();
}

void SettingsView::move_visible_up(std::size_t i)
{
    if (i == 0 || i >= nav_visible_.size()) return;
    std::swap(nav_visible_[i - 1], nav_visible_[i]);
    persist_nav_order();
}

void SettingsView::move_visible_down(std::size_t i)
{
    if (i + 1 >= nav_visible_.size()) return;
    std::swap(nav_visible_[i], nav_visible_[i + 1]);
    persist_nav_order();
}

void SettingsView::hide_screen_at(std::size_t i)
{
    if (nav_visible_.size() <= 1 || i >= nav_visible_.size()) return;
    nav_visible_.erase(nav_visible_.begin() + static_cast<std::ptrdiff_t>(i));
    persist_nav_order();
}

void SettingsView::restore_hidden(std::string const& name)
{
    nav_visible_.push_back(name);
    persist_nav_order();
}

void SettingsView::apply_nc_creds(NcLoginCredentials const& creds)
{
    std::string srv = creds.server_url;
    while (!srv.empty() && srv.back() == '/') srv.pop_back();
    (void)app_.secrets().set(CredentialKey::CALDAV_SERVER, srv);
    (void)app_.secrets().set(CredentialKey::CALDAV_USERNAME, creds.login_name);
    (void)app_.secrets().set(CredentialKey::CALDAV_PASSWORD, creds.app_password);
    caldav_server_.set_text(srv);
    caldav_user_.set_text(creds.login_name);
    caldav_password_.set_text(creds.app_password);
}

void SettingsView::on_nextcloud_login_clicked()
{
    std::string base(nc_server_entry_.get_text());
    auto init = app_.nextcloud_login_flow().initiate(base);
    Gtk::Window* parent = dynamic_cast<Gtk::Window*>(get_toplevel());
    if (!init.has_value()) {
        if (parent) {
            Gtk::MessageDialog dlg(*parent, "Could not start Nextcloud login flow.", false, Gtk::MESSAGE_ERROR,
                Gtk::BUTTONS_CLOSE);
            dlg.run();
        }
        return;
    }
    GError* err = nullptr;
    if (parent)
        gtk_show_uri_on_window(GTK_WINDOW(parent->gobj()), init->login_url.c_str(), GDK_CURRENT_TIME, &err);
    if (err) g_error_free(err);

    std::string poll_ep = init->poll_endpoint;
    std::string tok = init->poll_token;
    cd::NextcloudLoginFlow* flow = &app_.nextcloud_login_flow();
    SettingsView* self = this;
    std::thread([flow, poll_ep, tok, self]() {
        auto creds = flow->poll_blocking(poll_ep, tok);
        auto* pack = new std::pair<SettingsView*, std::optional<NcLoginCredentials>>(self, std::move(creds));
        g_idle_add(
            +[](gpointer data) -> gboolean {
                auto* p = static_cast<std::pair<SettingsView*, std::optional<NcLoginCredentials>>*>(data);
                if (p->second.has_value())
                    p->first->apply_nc_creds(*p->second);
                else {
                    Gtk::Window* w = dynamic_cast<Gtk::Window*>(p->first->get_toplevel());
                    if (w) {
                        Gtk::MessageDialog dlg(*w, "Nextcloud login timed out or was cancelled.", false,
                            Gtk::MESSAGE_WARNING, Gtk::BUTTONS_CLOSE);
                        dlg.run();
                    }
                }
                delete p;
                return FALSE;
            },
            pack);
    }).detach();
}

void SettingsView::on_discover_calendars_clicked()
{
    AppContainer* cap = &app_;
    SettingsView* self = this;
    std::thread([cap, self]() {
        try {
            auto cals = cap->caldav().discover_calendars();
            auto* payload =
                new std::pair<SettingsView*, std::vector<CalDavCalendar>>(self, std::move(cals));
            g_idle_add(
                +[](gpointer data) -> gboolean {
                    auto* p = static_cast<std::pair<SettingsView*, std::vector<CalDavCalendar>>*>(data);
                    p->first->populate_calendar_checks(std::move(p->second));
                    delete p;
                    return FALSE;
                },
                payload);
        }
        catch (...) {
            auto* payload = new std::pair<SettingsView*, bool>(self, false);
            g_idle_add(
                +[](gpointer data) -> gboolean {
                    auto* p = static_cast<std::pair<SettingsView*, bool>*>(data);
                    Gtk::Window* w = dynamic_cast<Gtk::Window*>(p->first->get_toplevel());
                    if (w) {
                        Gtk::MessageDialog dlg(*w,
                            "Calendar discovery failed (check CalDAV URL and credentials).", false,
                            Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
                        dlg.run();
                    }
                    delete p;
                    return FALSE;
                },
                payload);
        }
    }).detach();
}

void SettingsView::populate_calendar_checks(std::vector<CalDavCalendar> cals)
{
    cal_row_hrefs_.clear();
    for (Gtk::Widget* w : cal_checks_box_.get_children()) cal_checks_box_.remove(*w);

    auto selected = calendars_from_selected_json(app_.secrets().get(CredentialKey::CALDAV_SELECTED_CALENDARS));
    std::unordered_set<std::string> sel(selected.begin(), selected.end());

    for (auto const& cal : cals) {
        auto* row = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 6));
        auto* cb = Gtk::manage(new Gtk::CheckButton());
        std::string lab =
            cal.display_name.empty() ? cal.href : (cal.display_name + " — " + cal.href);
        auto* l = Gtk::manage(new Gtk::Label(lab));
        l->set_halign(Gtk::ALIGN_START);
        l->set_line_wrap(true);
        cb->set_active(sel.count(cal.href) > 0);
        row->pack_start(*cb, false, false);
        row->pack_start(*l, true, true);
        cal_checks_box_.pack_start(*row, false, false);
        cal_row_hrefs_.push_back({cb, cal.href});
    }
    cal_checks_box_.show_all();
}

void SettingsView::on_save_calendar_selection_clicked()
{
    nlohmann::json arr = nlohmann::json::array();
    for (auto const& pr : cal_row_hrefs_) {
        if (pr.first->get_active()) arr.push_back(pr.second);
    }
    (void)app_.secrets().set(CredentialKey::CALDAV_SELECTED_CALENDARS, arr.dump());

    Gtk::Window* win = dynamic_cast<Gtk::Window*>(get_toplevel());
    if (win) {
        Gtk::MessageDialog dlg(*win, "Saved calendar selection.", false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_CLOSE);
        dlg.run();
    }
}

void SettingsView::save_kanban_columns_csv()
{
    (void)app_.prefs().set_kanban_column_tags_csv(std::string(kanban_columns_csv_.get_text()));
}

} // namespace cd
