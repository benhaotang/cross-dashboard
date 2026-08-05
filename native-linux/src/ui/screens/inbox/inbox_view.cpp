#include "inbox_view.h"

#include "app_container.h"
#include "background/sync_scheduler.h"
#include "components/tag_flow.h"
#include "data/db/event_dao.h"
#include "data/db/issue_dao.h"
#include "data/db/task_dao.h"
#include "data/prefs/prefs.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <chrono>
#include <cstdio>
#include <regex>
#include <sstream>
#include <string>
#include <algorithm>

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

struct LocalDayBounds final {
    EpochMillis today_start{};
    EpochMillis tomorrow_start{};
    EpochMillis day_after_tomorrow_start{};
    EpochMillis week_start{};
    EpochMillis next_week_start{};
};

LocalDayBounds local_day_bounds()
{
    LocalDayBounds bounds{};
    GDateTime* now = g_date_time_new_now_local();
    if (!now) return bounds;
    GDateTime* today = g_date_time_new_local(
        g_date_time_get_year(now), g_date_time_get_month(now), g_date_time_get_day_of_month(now), 0, 0, 0);
    if (!today) {
        g_date_time_unref(now);
        return bounds;
    }
    GDateTime* tomorrow = g_date_time_add_days(today, 1);
    GDateTime* day_after = g_date_time_add_days(today, 2);
    GDateTime* week = g_date_time_add_days(today, -(g_date_time_get_day_of_week(today) - 1));
    GDateTime* next_week = g_date_time_add_days(week, 7);
    auto millis = [](GDateTime* value) -> EpochMillis {
        return value ? static_cast<EpochMillis>(g_date_time_to_unix(value)) * 1000 : 0;
    };
    bounds.today_start = millis(today);
    bounds.tomorrow_start = millis(tomorrow);
    bounds.day_after_tomorrow_start = millis(day_after);
    bounds.week_start = millis(week);
    bounds.next_week_start = millis(next_week);
    if (next_week) g_date_time_unref(next_week);
    if (week) g_date_time_unref(week);
    if (day_after) g_date_time_unref(day_after);
    if (tomorrow) g_date_time_unref(tomorrow);
    g_date_time_unref(today);
    g_date_time_unref(now);
    return bounds;
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
    for (auto const* label : {"All", "Events", "Tasks", "Today", "Tomorrow", "This Week", "Issues"})
        filter_combo_.append(label);
    filter_combo_.set_active(0);
    filter_combo_.signal_changed().connect(sigc::mem_fun(*this, &InboxView::on_filter_changed));
    if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(filter_combo_.gobj())))
        atk_object_set_name(a, "Filter inbox items");
    toolbar_.pack_start(filter_combo_, false, false);
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
    list_.set_activate_on_single_click(true);
    list_.signal_row_activated().connect(sigc::mem_fun(*this, &InboxView::on_row_activated));
    list_.set_tooltip_text("Open this item in its screen");
    scroll_.add(list_);

    total_line_.set_halign(Gtk::ALIGN_START);
    total_line_.set_line_wrap(true);

    pack_start(scroll_, true, true);
    pack_start(total_line_, false, false);

    atk_object_set_name(gtk_widget_get_accessible(GTK_WIDGET(list_.gobj())), "Inbox list");

    rebuild();
}

void InboxView::on_filter_changed()
{
    filter_index_ = filter_combo_.get_active_row_number();
    rebuild();
}

void InboxView::on_row_activated(Gtk::ListBoxRow* row)
{
    if (!row)
        return;
    int const index = gtk_list_box_row_get_index(GTK_LIST_BOX_ROW(row->gobj()));
    if (index < 0 || static_cast<std::size_t>(index) >= rows_.size())
        return;

    Row const& item = rows_[static_cast<std::size_t>(index)];
    if (std::holds_alternative<CalendarEvent>(item.data))
        signal_event_requested.emit(std::get<CalendarEvent>(item.data).uid);
    else if (std::holds_alternative<CalDavTask>(item.data))
        signal_task_requested.emit(std::get<CalDavTask>(item.data).uid);
    else if (std::holds_alternative<GiteaIssue>(item.data))
        signal_issue_requested.emit(std::get<GiteaIssue>(item.data).id);
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
        if (e.start > horizon)
            continue;
        EpochMillis const effective_end = e.end > e.start ? e.end : e.start;
        if (effective_end <= now)
            continue;
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
        for (auto const& category : t.categories) blob += " #" + category;
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
        for (auto const& label : iss.labels) blob += " #" + label;
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
    auto const magic_tags = planning_magic_tags(merged_app_preferences(app_.prefs()));

    auto const bounds = local_day_bounds();
    rows_.erase(std::remove_if(rows_.begin(), rows_.end(), [this, &bounds](Row const& row) {
        bool const is_event = std::holds_alternative<CalendarEvent>(row.data);
        bool const is_task = std::holds_alternative<CalDavTask>(row.data);
        bool const is_issue = std::holds_alternative<GiteaIssue>(row.data);
        if (filter_index_ == 1) return !is_event;
        if (filter_index_ == 2) return !is_task;
        if (filter_index_ == 6) return !is_issue;
        if (filter_index_ >= 3 && filter_index_ <= 5) {
            if (!is_task) return true;
            auto const& task = std::get<CalDavTask>(row.data);
            if (!task.due) return true;
            if (filter_index_ == 3)
                return *task.due < bounds.today_start || *task.due >= bounds.tomorrow_start;
            if (filter_index_ == 4)
                return *task.due < bounds.tomorrow_start || *task.due >= bounds.day_after_tomorrow_start;
            return *task.due < bounds.week_start || *task.due >= bounds.next_week_start;
        }
        return false;
    }), rows_.end());

    int sum_min = 0;
    for (auto const& r : rows_) sum_min += r.estimated_minutes;

    total_line_.set_text(fmt_time(sum_min));

    for (auto const& r : rows_) {
        auto* row = Gtk::manage(new Gtk::ListBoxRow);
        auto* content = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 4));
        auto* lab = Gtk::manage(new Gtk::Label());
        gchar* e1 = g_markup_escape_text(r.title.c_str(), -1);
        gchar* e2 = g_markup_escape_text(r.subtitle.c_str(), -1);
        if (e1 && e2)
            lab->set_markup(std::string(e1) + "\n<span size='smaller'><i>" + e2 + "</i></span>");
        if (e1) g_free(e1);
        if (e2) g_free(e2);
        lab->set_halign(Gtk::ALIGN_START);
        content->pack_start(*lab, false, false);

        std::vector<std::string> tags;
        if (std::holds_alternative<CalDavTask>(r.data))
            tags = std::get<CalDavTask>(r.data).categories;
        else if (std::holds_alternative<GiteaIssue>(r.data))
            tags = std::get<GiteaIssue>(r.data).labels;
        if (!tags.empty())
            content->pack_start(*make_tag_flow(std::move(tags), magic_tags), false, false);

        row->add(*content);
        list_.append(*row);
    }
    list_.show_all();
}

} // namespace cd
