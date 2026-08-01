#include "app_window.h"

#include "app_container.h"
#include "app_viewmodel.h"
#include "background/sync_scheduler.h"
#include "data/prefs/prefs.h"
#include "domain/models.h"
#include "screens/dashboard/dashboard_view.h"
#include "screens/events/events_view.h"
#include "screens/inbox/inbox_view.h"
#include "screens/issues/issues_view.h"
#include "screens/memos/memos_view.h"
#include "screens/notes/notes_view.h"
#include "screens/settings/settings_view.h"
#include "screens/tasks/tasks_view.h"
#include "screens/views/views_view.h"
#include "components/pomodoro_bar.h"
#include "components/pomodoro_modal.h"

extern "C" {
#include <handy.h>
#include <atk/atk.h>
#include <glib-object.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
}

#ifndef PACKAGE_DATADIR
#define PACKAGE_DATADIR ""
#endif

namespace cd {

namespace {

extern "C" void app_window_leaflet_folded_cb(GObject*, GParamSpec*, gpointer user)
{
    static_cast<AppWindow*>(user)->on_leaflet_folded();
}

extern "C" void app_window_sidebar_row_cb(GtkListBox*, GtkListBoxRow* row, gpointer user)
{
    static_cast<AppWindow*>(user)->on_screen_row(row);
}

extern "C" void app_window_sidebar_toggle_cb(GtkToggleButton*, gpointer user)
{
    static_cast<AppWindow*>(user)->on_sidebar_toggled();
}

} // namespace

AppWindow::AppWindow(AppContainer& app, AppViewModel& vm, SyncScheduler& sync)
    : Gtk::ApplicationWindow{}
    , app_(app)
    , vm_(vm)
    , sync_(sync)
{
    set_title("Cross-Dashboard");
    set_default_size(960, 640);
    add_events(Gdk::KEY_PRESS_MASK);
    set_can_focus(true);

    root_overlay_ = gtk_overlay_new();
    leaflet_ = GTK_WIDGET(hdy_leaflet_new());
    gtk_orientable_set_orientation(GTK_ORIENTABLE(leaflet_), GTK_ORIENTATION_HORIZONTAL);
    // In wide/unfolded mode the navigation rail keeps its requested width;
    // only the main content consumes extra horizontal space.
    hdy_leaflet_set_homogeneous(
        HDY_LEAFLET(leaflet_), FALSE, GTK_ORIENTATION_HORIZONTAL, FALSE);
    hdy_leaflet_set_transition_type(HDY_LEAFLET(leaflet_), HDY_LEAFLET_TRANSITION_TYPE_OVER);
    hdy_leaflet_set_can_swipe_back(HDY_LEAFLET(leaflet_), TRUE);
    hdy_leaflet_set_can_swipe_forward(HDY_LEAFLET(leaflet_), TRUE);

    sidebar_box_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(sidebar_box_, 240, -1);
    gtk_widget_set_hexpand(sidebar_box_, FALSE);
    gtk_style_context_add_class(gtk_widget_get_style_context(sidebar_box_), "cd-sidebar");

    sidebar_list_ = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(sidebar_list_), GTK_SELECTION_SINGLE);
    gtk_style_context_add_class(gtk_widget_get_style_context(sidebar_list_), "cd-nav-list");
    gtk_box_pack_start(GTK_BOX(sidebar_box_), sidebar_list_, TRUE, TRUE, 0);

    main_outer_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(main_outer_, 480, -1);
    gtk_widget_set_hexpand(main_outer_, TRUE);
    header_bar_ = GTK_WIDGET(hdy_header_bar_new());
    hdy_header_bar_set_show_close_button(HDY_HEADER_BAR(header_bar_), TRUE);
    hdy_header_bar_set_title(HDY_HEADER_BAR(header_bar_), "Cross-Dashboard");

    sidebar_toggle_btn_ = gtk_toggle_button_new();
    GtkWidget* sidebar_icon = gtk_image_new_from_icon_name("open-menu-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_container_add(GTK_CONTAINER(sidebar_toggle_btn_), sidebar_icon);
    // The button itself is excluded from the initial show_all() while the
    // leaflet is wide. Keep its child realized so showing the button after a
    // fold does not produce an empty square.
    gtk_widget_show(sidebar_icon);
    gtk_widget_set_tooltip_text(sidebar_toggle_btn_, "Show or hide navigation sidebar");
    gtk_widget_set_no_show_all(sidebar_toggle_btn_, TRUE);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(sidebar_toggle_btn_), "cd-icon-btn");
    if (AtkObject* a = gtk_widget_get_accessible(sidebar_toggle_btn_))
        atk_object_set_name(a, "Show or hide navigation sidebar");

    GtkWidget* pomodoro_btn = gtk_button_new_with_label("Pomodoro");
    gtk_widget_set_tooltip_text(pomodoro_btn, "Open Pomodoro timer");
    if (AtkObject* a = gtk_widget_get_accessible(pomodoro_btn))
        atk_object_set_name(a, "Open Pomodoro timer");

    hdy_header_bar_pack_start(HDY_HEADER_BAR(header_bar_), sidebar_toggle_btn_);
    hdy_header_bar_pack_end(HDY_HEADER_BAR(header_bar_), pomodoro_btn);

    // Use header_bar_ as the window titlebar — eliminates the duplicate OS titlebar.
    gtk_window_set_titlebar(GTK_WINDOW(gobj()), header_bar_);
    gtk_box_pack_start(GTK_BOX(main_outer_), GTK_WIDGET(stack_.gobj()), TRUE, TRUE, 0);

    hdy_leaflet_insert_child_after(HDY_LEAFLET(leaflet_), sidebar_box_, nullptr);
    hdy_leaflet_insert_child_after(HDY_LEAFLET(leaflet_), main_outer_, sidebar_box_);
    hdy_leaflet_set_visible_child(HDY_LEAFLET(leaflet_), main_outer_);

    pomodoro_bar_ = std::make_unique<PomodoroBar>(vm_);
    pomodoro_modal_ = std::make_unique<PomodoroModal>(vm_);

    gtk_container_add(GTK_CONTAINER(root_overlay_), leaflet_);
    gtk_overlay_add_overlay(GTK_OVERLAY(root_overlay_), pomodoro_bar_->widget());
    gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(root_overlay_), pomodoro_bar_->widget(), FALSE);
    gtk_container_add(GTK_CONTAINER(gobj()), root_overlay_);

    g_signal_connect(leaflet_, "notify::folded", G_CALLBACK(app_window_leaflet_folded_cb), this);
    g_signal_connect(leaflet_, "notify::visible-child", G_CALLBACK(app_window_leaflet_folded_cb), this);
    g_signal_connect(sidebar_list_, "row-selected", G_CALLBACK(app_window_sidebar_row_cb), this);
    g_signal_connect(sidebar_toggle_btn_, "toggled", G_CALLBACK(app_window_sidebar_toggle_cb), this);
    g_signal_connect(pomodoro_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer user) {
            static_cast<AppWindow*>(user)->vm_.set_pomodoro_modal_visible(true);
        }),
        this);

    vm_.signal_new_task_requested.connect(sigc::mem_fun(*this, &AppWindow::on_new_task_shortcut));
    vm_.signal_capture_initial_text.connect([this](std::string const&) {
        current_screen_key_ = "Capture";
        stack_.set_visible_child(Glib::ustring{"Capture"});
        hdy_header_bar_set_subtitle(HDY_HEADER_BAR(header_bar_), "Capture");
        show_main_content();
        if (memos_)
            memos_->rebuild();
    });
    vm_.signal_pomodoro_modal_visibility_changed.connect([this](bool visible) {
        if (visible)
            pomodoro_modal_->present(GTK_WINDOW(gobj()));
    });

    build_stack_pages();
    build_sidebar();

    sync_.signal_sync_completed.connect(sigc::mem_fun(*this, &AppWindow::refresh_current_screen));

    GtkListBoxRow* row0 = gtk_list_box_get_row_at_index(GTK_LIST_BOX(sidebar_list_), 0);
    if (row0) {
        gtk_list_box_select_row(GTK_LIST_BOX(sidebar_list_), row0);
        on_screen_row(row0);
    }

    gtk_widget_show_all(header_bar_);
    show_all_children();
    on_leaflet_folded();
    apply_theme();
}

void AppWindow::load_theme_css()
{
    static GtkCssProvider* provider = nullptr;
    if (!provider) {
        provider = gtk_css_provider_new();
    }
    static bool provider_registered = false;
    if (!provider_registered) {
        GdkScreen* screen = nullptr;
        if (gtk_widget_get_realized(GTK_WIDGET(gobj())))
            screen = gtk_widget_get_screen(GTK_WIDGET(gobj()));
        if (!screen) {
            GdkDisplay* disp = gdk_display_get_default();
            if (disp)
                screen = gdk_display_get_default_screen(disp);
        }
        if (screen)
            gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(provider),
                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        else
            gtk_style_context_add_provider(gtk_widget_get_style_context(GTK_WIDGET(gobj())),
                GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(provider);
        provider_registered = true;
    }
    AppSettings const s = merged_app_preferences(app_.prefs());
    bool const dark = (s.theme == ThemePreference::Dark);
    std::string path;
    if (PACKAGE_DATADIR[0] != '\0') {
        path = std::string(PACKAGE_DATADIR) + (dark ? "/themes/gtk-dark.css" : "/themes/gtk.css");
    }
    if (path.empty() || !g_file_test(path.c_str(), G_FILE_TEST_EXISTS)) {
        gchar* alt = g_build_filename(g_get_user_data_dir(), "cross-dashboard", "themes",
            dark ? "gtk-dark.css" : "gtk.css", nullptr);
        path = alt;
        g_free(alt);
    }
    GError* err = nullptr;
    if (!gtk_css_provider_load_from_path(provider, path.c_str(), &err)) {
        if (err)
            g_error_free(err);
    }
}

void AppWindow::apply_theme()
{
    AppSettings const s = merged_app_preferences(app_.prefs());
    GtkSettings* gs = gtk_settings_get_default();
    switch (s.theme) {
    case ThemePreference::Dark:
        g_object_set(gs, "gtk-application-prefer-dark-theme", TRUE, nullptr);
        break;
    case ThemePreference::Light:
        g_object_set(gs, "gtk-application-prefer-dark-theme", FALSE, nullptr);
        break;
    case ThemePreference::System:
    default:
        g_object_set(gs, "gtk-application-prefer-dark-theme", FALSE, nullptr);
        break;
    }
    load_theme_css();
}

void AppWindow::schedule_rebuild_navigation()
{
    g_idle_add(
        +[](gpointer data) -> gboolean {
            static_cast<AppWindow*>(data)->rebuild_navigation();
            return G_SOURCE_REMOVE;
        },
        this);
}

void AppWindow::rebuild_navigation()
{
    GList* const rows = gtk_container_get_children(GTK_CONTAINER(sidebar_list_));
    for (GList* l = rows; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(rows);

    std::vector<Gtk::Widget*> const kids = stack_.get_children();
    for (Gtk::Widget* w : kids)
        stack_.remove(*w);

    dash_ = nullptr;
    tasks_ = nullptr;
    events_ = nullptr;
    notes_ = nullptr;
    memos_ = nullptr;
    issues_ = nullptr;
    inbox_ = nullptr;
    views_ = nullptr;
    settings_ = nullptr;

    build_stack_pages();
    build_sidebar();

    GtkListBoxRow* row0 = gtk_list_box_get_row_at_index(GTK_LIST_BOX(sidebar_list_), 0);
    if (row0) {
        gtk_list_box_select_row(GTK_LIST_BOX(sidebar_list_), row0);
        on_screen_row(row0);
    }
}

bool AppWindow::on_key_press_event(GdkEventKey* ev)
{
    bool const ctrl = (ev->state & GDK_CONTROL_MASK) != 0;
    if (ctrl && ev->keyval == GDK_KEY_n) {
        on_new_task_shortcut();
        return true;
    }
    if (ctrl && ev->keyval == GDK_KEY_r) {
        sync_.sync_once();
        return true;
    }
    if (ctrl && ev->keyval == GDK_KEY_f) {
        focus_search_on_current_screen();
        return true;
    }
    return Gtk::ApplicationWindow::on_key_press_event(ev);
}

void AppWindow::focus_search_on_current_screen()
{
    if (current_screen_key_ == "Tasks" && tasks_)
        tasks_->focus_quick_input();
    else if (current_screen_key_ == "Notes" && notes_)
        notes_->focus_search();
    else if (current_screen_key_ == "Capture" && memos_)
        memos_->focus_search();
}

void AppWindow::refresh_current_screen()
{
    if (current_screen_key_.empty())
        return;

    char const* key = current_screen_key_.c_str();
    if (g_strcmp0(key, "Dashboard") == 0 && dash_)
        dash_->refresh();
    else if (g_strcmp0(key, "Tasks") == 0 && tasks_)
        tasks_->rebuild();
    else if (g_strcmp0(key, "Events") == 0 && events_)
        events_->refresh();
    else if (g_strcmp0(key, "Notes") == 0 && notes_)
        notes_->rebuild();
    else if (g_strcmp0(key, "Issues") == 0 && issues_)
        issues_->rebuild();
    else if (g_strcmp0(key, "Inbox") == 0 && inbox_)
        inbox_->rebuild();
    else if (g_strcmp0(key, "Views") == 0 && views_)
        views_->rebuild();
    else if (g_strcmp0(key, "Capture") == 0 && memos_)
        memos_->rebuild();
}

void AppWindow::build_stack_pages()
{
    AppSettings const cfg = merged_app_preferences(app_.prefs());

    for (std::string const& screen : cfg.visible_screens) {
        if (screen == "Dashboard") {
            dash_ = Gtk::manage(new DashboardView(app_, sync_));
            stack_.add(*dash_, Glib::ustring(screen));
        }
        else if (screen == "Tasks") {
            tasks_ = Gtk::manage(new TasksView(app_, vm_, sync_));
            stack_.add(*tasks_, Glib::ustring(screen));
        }
        else if (screen == "Events") {
            events_ = Gtk::manage(new EventsView(app_, sync_));
            stack_.add(*events_, Glib::ustring(screen));
        }
        else if (screen == "Notes") {
            notes_ = Gtk::manage(new NotesView(app_, sync_));
            stack_.add(*notes_, Glib::ustring(screen));
        }
        else if (screen == "Issues") {
            issues_ = Gtk::manage(new IssuesView(app_, sync_));
            stack_.add(*issues_, Glib::ustring(screen));
        }
        else if (screen == "Inbox") {
            inbox_ = Gtk::manage(new InboxView(app_, sync_));
            stack_.add(*inbox_, Glib::ustring(screen));
        }
        else if (screen == "Views") {
            views_ = Gtk::manage(new ViewsView(app_, sync_));
            stack_.add(*views_, Glib::ustring(screen));
        }
        else if (screen == "Settings") {
            settings_ = Gtk::manage(new SettingsView(app_, sync_,
                [this] { apply_theme(); },
                [this] { schedule_rebuild_navigation(); }));
            stack_.add(*settings_, Glib::ustring(screen));
        }
        else if (screen == "Capture") {
            memos_ = Gtk::manage(new MemosView(app_, vm_, sync_));
            stack_.add(*memos_, Glib::ustring(screen));
        }
        else {
            auto* ph = Gtk::manage(new Gtk::Label(
                "This screen is not available yet (Capture = Phase 6)."));
            ph->set_line_wrap(true);
            ph->set_margin_start(12);
            ph->set_margin_top(12);
            stack_.add(*ph, Glib::ustring(screen));
        }
    }
}

void AppWindow::build_sidebar()
{
    auto screen_icon = [](std::string const& name) -> const char* {
        if (name == "Dashboard") return "go-home-symbolic";
        if (name == "Inbox")     return "mail-unread-symbolic";
        if (name == "Events")    return "x-office-calendar-symbolic";
        if (name == "Tasks")     return "emblem-default-symbolic";
        if (name == "Notes")     return "document-edit-symbolic";
        if (name == "Issues")    return "emblem-important-symbolic";
        if (name == "Views")     return "view-list-symbolic";
        if (name == "Capture")   return "camera-photo-symbolic";
        if (name == "Settings")  return "preferences-system-symbolic";
        return "applications-other-symbolic";
    };

    AppSettings const cfg = merged_app_preferences(app_.prefs());
    for (std::string const& screen : cfg.visible_screens) {
        GtkWidget* row = gtk_list_box_row_new();
        gtk_style_context_add_class(gtk_widget_get_style_context(row), "cd-nav-row");

        GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_margin_start(hbox, 10);
        gtk_widget_set_margin_end(hbox, 10);
        gtk_widget_set_margin_top(hbox, 7);
        gtk_widget_set_margin_bottom(hbox, 7);

        GtkWidget* icon = gtk_image_new_from_icon_name(screen_icon(screen), GTK_ICON_SIZE_SMALL_TOOLBAR);
        gtk_style_context_add_class(gtk_widget_get_style_context(icon), "cd-nav-icon");

        GtkWidget* lab = gtk_label_new(screen.c_str());
        gtk_widget_set_halign(lab, GTK_ALIGN_START);
        gtk_widget_set_hexpand(lab, TRUE);

        gtk_box_pack_start(GTK_BOX(hbox), icon, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), lab, TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(row), hbox);

        g_object_set_data_full(G_OBJECT(row), "cd-screen", g_strdup(screen.c_str()), g_free);
        if (AtkObject* a = gtk_widget_get_accessible(row))
            atk_object_set_name(a, ("Screen: " + screen).c_str());

        gtk_widget_show_all(row);
        gtk_container_add(GTK_CONTAINER(sidebar_list_), row);
    }
}

void AppWindow::on_screen_row(GtkListBoxRow* row)
{
    if (!row)
        return;

    char const* key = reinterpret_cast<char const*>(g_object_get_data(G_OBJECT(row), "cd-screen"));
    if (!key)
        return;

    current_screen_key_ = key;

    stack_.set_visible_child(Glib::ustring{key});
    hdy_header_bar_set_subtitle(HDY_HEADER_BAR(header_bar_), key);

    refresh_current_screen();
    show_main_content();
}

void AppWindow::update_sidebar_toggle(bool active)
{
    updating_sidebar_toggle_ = true;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sidebar_toggle_btn_), active ? TRUE : FALSE);
    updating_sidebar_toggle_ = false;
}

void AppWindow::show_main_content()
{
    if (!hdy_leaflet_get_folded(HDY_LEAFLET(leaflet_))) return;
    hdy_leaflet_set_visible_child(HDY_LEAFLET(leaflet_), main_outer_);
    update_sidebar_toggle(false);
}

void AppWindow::on_sidebar_toggled()
{
    if (updating_sidebar_toggle_ || !hdy_leaflet_get_folded(HDY_LEAFLET(leaflet_))) return;
    bool const show_sidebar =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sidebar_toggle_btn_)) != FALSE;
    hdy_leaflet_set_visible_child(
        HDY_LEAFLET(leaflet_), show_sidebar ? sidebar_box_ : main_outer_);
}

void AppWindow::on_leaflet_folded()
{
    bool const folded = hdy_leaflet_get_folded(HDY_LEAFLET(leaflet_)) != FALSE;
    gtk_widget_set_visible(sidebar_toggle_btn_, folded ? TRUE : FALSE);
    if (!folded) {
        update_sidebar_toggle(false);
        return;
    }

    GtkWidget* visible = hdy_leaflet_get_visible_child(HDY_LEAFLET(leaflet_));
    bool const sidebar_visible = visible == sidebar_box_;
    update_sidebar_toggle(sidebar_visible);
}

void AppWindow::on_new_task_shortcut()
{
    current_screen_key_ = "Tasks";
    stack_.set_visible_child(Glib::ustring{"Tasks"});
    hdy_header_bar_set_subtitle(HDY_HEADER_BAR(header_bar_), "Tasks");
    show_main_content();
    if (tasks_)
        tasks_->focus_quick_input();
}

} // namespace cd
