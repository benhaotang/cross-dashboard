#include "dashboard_view.h"

#include "app_container.h"
#include "background/sync_scheduler.h"
#include "data/db/event_dao.h"
#include "data/db/issue_dao.h"
#include "data/db/task_dao.h"
#include "domain/models.h"
#include "data/repository/repositories.h"

#include <glib.h>
#include <gtk/gtk.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <map>
#include <string>

#include <gtkmm/frame.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/levelbar.h>
#include <gtkmm/separator.h>

namespace cd {

namespace {

EpochMillis millis_now_wall()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

constexpr std::int64_t kMsPerDay64 = std::int64_t{86400} * std::int64_t{1000};

Gtk::Box* make_stat_pillar(int value, char const* caption, char const* style_class)
{
    auto* col = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 4));
    col->set_valign(Gtk::ALIGN_START);
    col->get_style_context()->add_class("cd-dash-pill");

    auto* num = Gtk::manage(new Gtk::Label());
    num->set_markup("<span size='24000' weight='bold'>" + std::to_string(value) + "</span>");
    num->set_halign(Gtk::ALIGN_CENTER);

    auto* cap = Gtk::manage(new Gtk::Label(caption));
    cap->set_halign(Gtk::ALIGN_CENTER);
    cap->get_style_context()->add_class("cd-dash-pill-caption");
    if (style_class && *style_class)
        num->get_style_context()->add_class(style_class);

    col->pack_start(*num, false, false);
    col->pack_start(*cap, false, false);
    return col;
}

} // namespace

DashboardView::DashboardView(AppContainer& app, SyncScheduler& sync)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0)
    , app_(app)
    , sync_(sync)
{
    toolbar_.get_style_context()->add_class("cd-toolbar");
    refresh_btn_.set_image_from_icon_name("view-refresh-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    refresh_btn_.set_tooltip_text("Sync from server and refresh dashboard");
    refresh_btn_.set_relief(Gtk::RELIEF_NONE);
    refresh_btn_.get_style_context()->add_class("cd-icon-btn");
    if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(refresh_btn_.gobj())))
        atk_object_set_name(a, "Sync and refresh dashboard");
    refresh_btn_.signal_clicked().connect([this] {
        sync_.sync_once();
        refresh();
    });
    toolbar_.pack_end(refresh_btn_, false, false);

    scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    content_.set_margin_start(20);
    content_.set_margin_end(20);
    content_.set_margin_top(16);
    content_.set_margin_bottom(20);
    scroll_.add(content_);

    pack_start(toolbar_, false, false);
    pack_start(scroll_, true, true);

    refresh();
}

void DashboardView::refresh()
{
    for (Gtk::Widget* w : content_.get_children())
        content_.remove(*w);

    EpochMillis const now_ms = millis_now_wall();
    auto const stats_rows = app_.stats().range_starting_days_ago(7);
    std::map<std::string, DailyStats const*> by_date;
    for (auto const& r : stats_rows)
        by_date[r.date_iso] = &r;

    int total_tasks_week = 0;
    int total_pom_week = 0;
    for (auto const& r : stats_rows) {
        total_tasks_week += r.tasks_completed;
        total_pom_week += r.pomodoro_sessions;
    }

    IssueDao idao(app_.db());
    int const open_issues = static_cast<int>(idao.get_by_state("open").size());

    // ── This week (stats + chart) ─────────────────────────────────
    auto* week_outer = Gtk::manage(new Gtk::Frame());
    week_outer->set_label("This week");
    week_outer->set_label_align(0.0f, 0.5f);
    week_outer->get_style_context()->add_class("cd-dash-frame");

    auto* week_pad = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 14));
    week_pad->set_margin_start(12);
    week_pad->set_margin_end(12);
    week_pad->set_margin_top(10);
    week_pad->set_margin_bottom(12);

    auto* pills = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 12));
    pills->pack_start(*make_stat_pillar(total_tasks_week, "Tasks done", "cd-dash-metric-green"), false, false);
    pills->pack_start(*make_stat_pillar(total_pom_week, "Pomodoros", "cd-dash-metric-orange"), false, false);
    pills->pack_start(*make_stat_pillar(open_issues, "Open issues", "cd-dash-metric-blue"), false, false);
    week_pad->pack_start(*pills, false, false);

    int max_bar = 1;
    struct DayBar {
        std::string day_label;
        int tasks{};
        int pom{};
    };
    std::vector<DayBar> days;
    GDateTime* now_dt = g_date_time_new_now_local();
    if (now_dt) {
        for (int off = 6; off >= 0; --off) {
            GDateTime* day = g_date_time_add_days(now_dt, -off);
            if (!day)
                continue;
            gchar* iso = g_date_time_format(day, "%Y-%m-%d");
            gchar* dlab = g_date_time_format(day, "%a");
            DayBar db;
            db.day_label = dlab ? dlab : "?";
            if (iso) {
                auto it = by_date.find(iso);
                if (it != by_date.end() && it->second) {
                    db.tasks = it->second->tasks_completed;
                    db.pom = it->second->pomodoro_sessions;
                }
                g_free(iso);
            }
            if (dlab)
                g_free(dlab);
            max_bar = std::max(max_bar, std::max(db.tasks, db.pom));
            days.push_back(db);
            g_date_time_unref(day);
        }
        g_date_time_unref(now_dt);
    }

    auto* chart_title = Gtk::manage(new Gtk::Label("Last 7 days"));
    chart_title->get_style_context()->add_class("cd-dash-section-hint");
    chart_title->set_halign(Gtk::ALIGN_START);
    week_pad->pack_start(*chart_title, false, false);

    auto* chart_grid = Gtk::manage(new Gtk::Grid());
    chart_grid->set_row_spacing(6);
    chart_grid->set_column_spacing(8);
    chart_grid->set_column_homogeneous(true);

    int col = 0;
    for (auto const& d : days) {
        auto* dl = Gtk::manage(new Gtk::Label(d.day_label));
        dl->set_halign(Gtk::ALIGN_CENTER);
        dl->get_style_context()->add_class("cd-dash-day-lab");

        auto* tb = Gtk::manage(new Gtk::LevelBar());
        tb->set_min_value(0);
        tb->set_max_value(static_cast<double>(max_bar));
        tb->set_value(static_cast<double>(d.tasks));
        tb->set_size_request(-1, 10);
        tb->get_style_context()->add_class("cd-dash-level-tasks");

        auto* pb = Gtk::manage(new Gtk::LevelBar());
        pb->set_min_value(0);
        pb->set_max_value(static_cast<double>(max_bar));
        pb->set_value(static_cast<double>(d.pom));
        pb->set_size_request(-1, 10);
        pb->get_style_context()->add_class("cd-dash-level-pom");

        auto* vcol = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 4));
        vcol->pack_start(*dl, false, false);
        vcol->pack_start(*tb, false, false);
        vcol->pack_start(*pb, false, false);

        chart_grid->attach(*vcol, col, 0, 1, 1);
        ++col;
    }
    week_pad->pack_start(*chart_grid, false, false);
    week_outer->add(*week_pad);
    content_.pack_start(*week_outer, false, false);

    // ── Upcoming events | Due soon ───────────────────────────────
    auto* mid = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 16));
    mid->set_homogeneous(true);

    EventDao evdao(app_.db());
    auto upcoming = evdao.get_upcoming(now_ms, 5);

    auto* ev_frame = Gtk::manage(new Gtk::Frame());
    ev_frame->set_label("Upcoming events");
    ev_frame->set_label_align(0.0f, 0.5f);
    ev_frame->get_style_context()->add_class("cd-dash-frame");
    auto* ev_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 6));
    ev_box->set_margin_start(12);
    ev_box->set_margin_end(12);
    ev_box->set_margin_top(10);
    ev_box->set_margin_bottom(12);
    if (upcoming.empty()) {
        auto* empty = Gtk::manage(new Gtk::Label("No upcoming events"));
        empty->set_halign(Gtk::ALIGN_CENTER);
        empty->get_style_context()->add_class("cd-dash-empty");
        empty->set_margin_top(24);
        empty->set_margin_bottom(24);
        ev_box->pack_start(*empty, true, true);
    }
    else {
        for (std::size_t i = 0; i < upcoming.size(); ++i) {
            auto const& e = upcoming[i];
            gchar* esc = g_markup_escape_text(e.summary.c_str(), -1);
            GDateTime* sdt = g_date_time_new_from_unix_local(static_cast<gint64>(e.start / 1000));
            gchar* when{};
            if (sdt)
                when = g_date_time_format(sdt, "%b %e · %H:%M");
            std::string line = esc ? esc : "";
            if (when) {
                gchar* wesc = g_markup_escape_text(when, -1);
                line += "\n<span size='smaller' alpha='60%'>" + std::string(wesc ? wesc : "") + "</span>";
                if (wesc) g_free(wesc);
                g_free(when);
            }
            if (sdt) g_date_time_unref(sdt);
            if (esc) g_free(esc);

            auto* row = Gtk::manage(new Gtk::Label());
            row->set_markup(line);
            row->set_halign(Gtk::ALIGN_START);
            row->set_line_wrap(true);
            ev_box->pack_start(*row, false, false);
            if (i + 1 < upcoming.size()) {
                auto* sep = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
                sep->set_margin_top(4);
                sep->set_margin_bottom(4);
                ev_box->pack_start(*sep, false, false);
            }
        }
    }
    ev_frame->add(*ev_box);
    mid->pack_start(*ev_frame, true, true);

    TaskDao tdao(app_.db());
    EpochMillis const due_deadline = now_ms + 7LL * kMsPerDay64;
    auto due_list = tdao.get_due_soon(due_deadline, 25);

    auto* due_frame = Gtk::manage(new Gtk::Frame());
    due_frame->set_label("Due soon");
    due_frame->set_label_align(0.0f, 0.5f);
    due_frame->get_style_context()->add_class("cd-dash-frame");
    auto* due_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 6));
    due_box->set_margin_start(12);
    due_box->set_margin_end(12);
    due_box->set_margin_top(10);
    due_box->set_margin_bottom(12);
    if (due_list.empty()) {
        auto* empty = Gtk::manage(new Gtk::Label("No tasks due soon"));
        empty->set_halign(Gtk::ALIGN_CENTER);
        empty->get_style_context()->add_class("cd-dash-empty");
        empty->set_margin_top(24);
        empty->set_margin_bottom(24);
        due_box->pack_start(*empty, true, true);
    }
    else {
        for (std::size_t i = 0; i < due_list.size(); ++i) {
            auto const& t = due_list[i];
            gchar* esc = g_markup_escape_text(t.summary.c_str(), -1);
            std::string line = esc ? esc : "";
            if (esc) g_free(esc);
            if (t.due) {
                GDateTime* ddt = g_date_time_new_from_unix_local(static_cast<gint64>(*t.due / 1000));
                if (ddt) {
                    gchar* due_s = g_date_time_format(ddt, "%b %e · %H:%M");
                    if (due_s) {
                        gchar* desc = g_markup_escape_text(due_s, -1);
                        line += "\n<span size='smaller' alpha='60%'>" + std::string(desc ? desc : "") + "</span>";
                        if (desc) g_free(desc);
                        g_free(due_s);
                    }
                    if (*t.due < now_ms)
                        line += "\n<span size='smaller' color='#c01c28'>Overdue</span>";
                    g_date_time_unref(ddt);
                }
            }
            auto* row = Gtk::manage(new Gtk::Label());
            row->set_markup(line);
            row->set_halign(Gtk::ALIGN_START);
            row->set_line_wrap(true);
            due_box->pack_start(*row, false, false);
            if (i + 1 < due_list.size()) {
                auto* sep = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
                sep->set_margin_top(4);
                sep->set_margin_bottom(4);
                due_box->pack_start(*sep, false, false);
            }
        }
    }
    due_frame->add(*due_box);
    mid->pack_start(*due_frame, true, true);
    content_.pack_start(*mid, false, false);

    // ── Open issues ───────────────────────────────────────────────
    auto* iss_frame = Gtk::manage(new Gtk::Frame());
    iss_frame->set_label("Gitea issues");
    iss_frame->set_label_align(0.0f, 0.5f);
    iss_frame->get_style_context()->add_class("cd-dash-frame");
    auto* iss_pad = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 12));
    iss_pad->set_margin_start(12);
    iss_pad->set_margin_end(12);
    iss_pad->set_margin_top(12);
    iss_pad->set_margin_bottom(12);

    auto* big = Gtk::manage(new Gtk::Label());
    big->set_markup("<span size='36000' weight='bold'>" + std::to_string(open_issues) + "</span>");
    big->set_valign(Gtk::ALIGN_CENTER);

    auto* subcol = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 2));
    auto* t1 = Gtk::manage(new Gtk::Label("Open issues"));
    t1->get_style_context()->add_class("cd-dash-issues-title");
    auto* t2 = Gtk::manage(new Gtk::Label("Across configured repositories"));
    t2->get_style_context()->add_class("cd-dash-section-hint");
    t2->set_halign(Gtk::ALIGN_START);
    t1->set_halign(Gtk::ALIGN_START);
    subcol->pack_start(*t1, false, false);
    subcol->pack_start(*t2, false, false);

    iss_pad->pack_start(*big, false, false);
    iss_pad->pack_start(*subcol, false, false);
    iss_frame->add(*iss_pad);
    content_.pack_start(*iss_frame, false, false);

    content_.show_all();
}

} // namespace cd
