#include "dashboard_view.h"

#include "app_container.h"
#include "data/db/event_dao.h"
#include "data/db/issue_dao.h"
#include "data/db/task_dao.h"
#include "data/repository/repositories.h"

#include <glib.h>
#include <gtk/gtk.h>

#include <chrono>
#include <limits>
#include <sstream>
#include <string>

namespace cd {

namespace {

EpochMillis millis_now_wall()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

constexpr std::int64_t kMsPerDay64 = std::int64_t{86400} * std::int64_t{1000};

} // namespace

DashboardView::DashboardView(AppContainer& app)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 12)
    , app_(app)
    , headline_("")
{
    headline_.set_halign(Gtk::ALIGN_START);
    headline_.set_line_wrap(true);
    pack_start(headline_, false, false);
    atk_object_set_name(gtk_widget_get_accessible(GTK_WIDGET(headline_.gobj())),
        "Dashboard summary");
}

void DashboardView::refresh()
{
    EpochMillis const now_ms = millis_now_wall();

    auto stats_summary = [&](std::ostringstream& oss) {
        auto rows = app_.stats().range_starting_days_ago(7);
        int tc = 0, pom = 0, iss = 0;
        for (auto const& r : rows) {
            tc += r.tasks_completed;
            pom += r.pomodoro_sessions;
            iss += r.issues_closed;
        }
        oss << "<b>Past week</b>\n"
            << "Tasks completed: " << tc << '\n'
            << "Pomodoros logged: " << pom << '\n'
            << "Issues closed: " << iss << "\n\n";
    };

    auto events_txt = [&](std::ostringstream& oss) {
        EventDao evdao(app_.db());
        auto evs = evdao.get_upcoming(now_ms, 3);
        oss << "<b>Upcoming events</b>\n";
        if (evs.empty()) {
            oss << "(none cached — background sync arrives in Phase&nbsp;4)\n\n";
            return;
        }
        for (auto const& e : evs) {
            gchar* tl = g_markup_escape_text(e.summary.c_str(), -1);
            if (tl) {
                oss << "• " << tl << '\n';
                g_free(tl);
            }
        }
        oss << '\n';
    };

    auto tasks_today = [&](std::ostringstream& oss) {
        GDateTime* now = g_date_time_new_now_local();
        if (!now) {
            oss << "<b>Tasks due today</b>: ?\n\n";
            return;
        }
        gint y{}, m{}, d{};
        g_date_time_get_ymd(now, &y, &m, &d);
        GDateTime* day_start = g_date_time_new_local(y, m, d, 0, 0, 0.0);
        g_date_time_unref(now);
        EpochMillis ds_begin = std::numeric_limits<EpochMillis>::min();
        if (day_start) {
            gint64 unix_s = g_date_time_to_unix(day_start);
            ds_begin = unix_s * 1000;
            g_date_time_unref(day_start);
        }
        EpochMillis const ds_end = ds_begin == std::numeric_limits<EpochMillis>::min()
                                       ? ds_begin - 1
                                       : ds_begin + kMsPerDay64 - 1;

        TaskDao tdao(app_.db());
        auto const all = tdao.get_all();
        int cnt = 0;
        std::ostringstream lines;
        if (ds_begin != std::numeric_limits<EpochMillis>::min()) {
            for (auto const& t : all) {
                if (!t.due) continue;
                if (*t.due >= ds_begin && *t.due <= ds_end) {
                    ++cnt;
                    gchar* mt = g_markup_escape_text(t.summary.c_str(), -1);
                    if (mt) {
                        lines << "• " << mt << '\n';
                        g_free(mt);
                    }
                }
            }
        }

        oss << "<b>Tasks due today</b>: " << cnt << '\n';
        if (cnt)
            oss << lines.str();

        oss << '\n';
    };

    auto issues_txt = [&](std::ostringstream& oss) {
        IssueDao idao(app_.db());
        auto const open = idao.get_by_state("open");
        oss << "<b>Open issues</b>: " << open.size() << "\n";
    };

    std::ostringstream body;
    stats_summary(body);
    events_txt(body);
    tasks_today(body);
    issues_txt(body);

    headline_.set_markup(body.str());
}

} // namespace cd
