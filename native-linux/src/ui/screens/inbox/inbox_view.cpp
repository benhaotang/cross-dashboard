#include "inbox_view.h"

#include "app_container.h"
#include "background/sync_scheduler.h"
#include "data/db/event_dao.h"
#include "data/db/issue_dao.h"
#include "data/db/task_dao.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <chrono>
#include <cstdio>
#include <regex>
#include <sstream>
#include <string>

namespace cd {

namespace {

EpochMillis millis_now_wall()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

/** Sum minutes from `#12m` and `#2h` tags (case-insensitive). */
int estimate_from_tags(std::string const& blob)
{
    int m = 0;
    try {
        std::regex const hm(R"((^|\s)#(\d+)\s*h\b)", std::regex::icase);
        std::regex const mm(R"((^|\s)#(\d+)\s*m\b)", std::regex::icase);
        for (std::sregex_iterator i(blob.begin(), blob.end(), hm), e; i != e; ++i)
            m += std::stoi((*i)[2].str()) * 60;
        for (std::sregex_iterator i(blob.begin(), blob.end(), mm), e; i != e; ++i)
            m += std::stoi((*i)[2].str());
    }
    catch (...) {}
    return m;
}

std::string fmt_time(int total_minutes)
{
    int h = total_minutes / 60;
    int m = total_minutes % 60;
    std::ostringstream o;
    o << "Estimated time: ";
    if (h) o << h << "h ";
    o << m << "m";
    return o.str();
}

} // namespace

InboxView::InboxView(AppContainer& app, SyncScheduler& sync)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 6)
    , app_(app)
    , sync_(sync)
    , scroll_{}
    , list_{}
    , total_line_("")
{
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(toolbar_.gobj())), "cd-toolbar");
    refresh_btn_.set_image_from_icon_name("view-refresh-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    refresh_btn_.set_tooltip_text("Sync from server and refresh inbox");
    refresh_btn_.set_relief(Gtk::RELIEF_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(refresh_btn_.gobj())), "cd-icon-btn");
    if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(refresh_btn_.gobj())))
        atk_object_set_name(a, "Sync and refresh inbox");
    refresh_btn_.signal_clicked().connect([this] {
        sync_.sync_once();
        rebuild();
    });
    toolbar_.pack_end(refresh_btn_, false, false);
    pack_start(toolbar_, false, false);

    scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    list_.set_selection_mode(Gtk::SELECTION_SINGLE);
    scroll_.add(list_);

    total_line_.set_halign(Gtk::ALIGN_START);
    total_line_.set_line_wrap(true);

    pack_start(scroll_, true, true);
    pack_start(total_line_, false, false);

    atk_object_set_name(gtk_widget_get_accessible(GTK_WIDGET(list_.gobj())), "Inbox list");

    rebuild();
}

void InboxView::populate_rows(std::vector<Row>& out)
{
    EventDao evdao(app_.db());
    TaskDao tdao(app_.db());
    IssueDao isdao(app_.db());

    EpochMillis const now = millis_now_wall();
    constexpr std::int64_t kWeek = 7LL * 86400LL * 1000LL;
    EpochMillis const horizon = now + kWeek;

    for (auto const& e : evdao.get_all()) {
        if (e.start > horizon) continue;
        Row r{};
        r.data = e;
        r.title = e.summary;
        GDateTime* sdt = g_date_time_new_from_unix_local(static_cast<gint64>(e.start / 1000));
        if (sdt) {
            gchar* ts = g_date_time_format(sdt, "%F %R");
            if (ts) {
                r.subtitle = std::string("Event · ") + ts;
                g_free(ts);
            }
            g_date_time_unref(sdt);
        }
        else r.subtitle = "Event";
        std::string blob = e.summary;
        if (e.description) blob += *e.description;
        r.estimated_minutes = estimate_from_tags(blob);
        out.push_back(std::move(r));
    }

    for (auto const& t : tdao.get_all()) {
        if (t.status == TaskStatus::Completed) continue;
        Row r{};
        r.data = t;
        r.title = t.summary;
        if (t.due) {
            GDateTime* dt =
                g_date_time_new_from_unix_local(static_cast<gint64>(*t.due / 1000));
            if (dt) {
                gchar* ts = g_date_time_format(dt, "%F %R");
                if (ts) {
                    r.subtitle = std::string("Task · due ") + ts;
                    g_free(ts);
                }
                g_date_time_unref(dt);
            }
        }
        else r.subtitle = "Task";
        std::string blob = t.summary + " ";
        if (t.description) blob += *t.description;
        r.estimated_minutes = estimate_from_tags(blob);
        out.push_back(std::move(r));
    }

    for (auto const& iss : isdao.get_all()) {
        if (iss.state != "open") continue;
        Row r{};
        r.data = iss;
        r.title = iss.title;
        r.subtitle = "Issue · " + iss.repository;
        std::string blob = iss.title + " " + iss.body;
        r.estimated_minutes = estimate_from_tags(blob);
        out.push_back(std::move(r));
    }

    std::stable_sort(out.begin(), out.end(),
        [](Row const& a, Row const& b) { return a.title < b.title; });
}

void InboxView::rebuild()
{
    for (Gtk::Widget* rw : list_.get_children()) list_.remove(*rw);
    rows_.clear();

    populate_rows(rows_);

    int sum_min = 0;
    for (auto const& r : rows_) sum_min += r.estimated_minutes;

    total_line_.set_text(fmt_time(sum_min));

    for (auto const& r : rows_) {
        auto* row = Gtk::manage(new Gtk::ListBoxRow);
        auto* lab = Gtk::manage(new Gtk::Label());
        gchar* e1 = g_markup_escape_text(r.title.c_str(), -1);
        gchar* e2 = g_markup_escape_text(r.subtitle.c_str(), -1);
        if (e1 && e2)
            lab->set_markup(std::string(e1) + "\n<span size='smaller'><i>" + e2 + "</i></span>");
        if (e1) g_free(e1);
        if (e2) g_free(e2);
        lab->set_halign(Gtk::ALIGN_START);
        row->add(*lab);
        list_.append(*row);
    }
    list_.show_all();
}

} // namespace cd
