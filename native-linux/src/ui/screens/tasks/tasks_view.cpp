#include "tasks_view.h"

#include "app_container.h"
#include "app_viewmodel.h"
#include "background/sync_scheduler.h"
#include "data/db/task_dao.h"
#include "data/parser/task_input_parser.h"
#include "data/prefs/prefs.h"
#include "data/repository/repositories.h"

#include <glib.h>
#include <gtk/gtk.h>

#include <gtkmm/checkbutton.h>
#include <gtkmm/label.h>
#include <gtkmm/messagedialog.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace cd {

namespace {

EpochMillis millis_now_wall()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

static std::string priority_label_text(int prio)
{
    if (prio == 1)
        return "!!!";
    if (prio == 5)
        return "!!";
    if (prio == 9)
        return "!";
    return {};
}

static std::optional<std::string> due_line(CalDavTask const& task)
{
    if (!task.due) return std::nullopt;
    GDateTime* dt =
        g_date_time_new_from_unix_local(static_cast<long>(static_cast<long long>(*task.due) / 1000LL));
    if (!dt)
        return std::nullopt;
    gchar* s = g_date_time_format(dt, "%F %R");
    g_date_time_unref(dt);
    if (!s)
        return std::nullopt;
    std::string out{s};
    g_free(s);
    return out;
}

} // namespace

TasksView::TasksView(AppContainer& app, AppViewModel& vm, SyncScheduler& sync)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 6)
    , app_(app)
    , vm_(vm)
    , sync_(sync)
    , scroll_{}
{
    vm_.signal_new_task_requested.connect([this]() { focus_quick_input(); });

    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(toolbar_.gobj())), "cd-toolbar");
    refresh_btn_.set_image_from_icon_name("view-refresh-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    refresh_btn_.set_tooltip_text("Sync from server and refresh tasks");
    refresh_btn_.set_relief(Gtk::RELIEF_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(refresh_btn_.gobj())), "cd-icon-btn");
    if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(refresh_btn_.gobj())))
        atk_object_set_name(a, "Sync and refresh tasks");
    refresh_btn_.signal_clicked().connect([this] {
        sync_.sync_once();
        rebuild();
    });
    toolbar_.pack_end(refresh_btn_, false, false);
    pack_start(toolbar_, false, false);

    scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    list_.set_selection_mode(Gtk::SELECTION_SINGLE);

    scroll_.add(list_);
    pack_start(scroll_, true, true);
    pack_start(input_, false, false);

    input_.signal_submit_requested.connect(sigc::mem_fun(*this, &TasksView::on_quick_submit));
    rebuild();
}

void TasksView::focus_quick_input()
{
    input_.grab_entry_focus();
}

Gtk::ListBoxRow* TasksView::make_row(CalDavTask const& task, int depth)
{
    auto* row = Gtk::manage(new Gtk::ListBoxRow);
    auto* hb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 8));

    auto* chk = Gtk::manage(new Gtk::CheckButton());

    chk->set_active(task.status == TaskStatus::Completed);

    auto* lbl = Gtk::manage(new Gtk::Label());

    gchar* escaped = g_markup_escape_text(task.summary.c_str(), -1);
    Glib::ustring markup = escaped ? escaped : "";
    if (escaped) g_free(escaped);

    if (auto dl = due_line(task)) {
        gchar* es = g_markup_escape_text(dl->c_str(), -1);
        markup += "\n<span size='smaller'><i>" + Glib::ustring(es ? es : "") + "</i></span>";
        if (es) g_free(es);
    }

    lbl->set_markup(markup);

    lbl->set_halign(Gtk::ALIGN_START);
    lbl->set_margin_start(8 + depth * 20);

    if (std::string const chips = priority_label_text(task.priority); !chips.empty()) {
        auto* pch = Gtk::manage(new Gtk::Label(chips));
        pch->set_margin_start(depth * 20);
        hb->pack_start(*pch, false, false);
    }
    hb->pack_start(*chk, false, false);
    hb->pack_start(*lbl, true, true);

    atk_object_set_name(gtk_widget_get_accessible(GTK_WIDGET(chk->gobj())), "Task complete checkbox");

    std::string const uid_capture = task.uid;

    chk->signal_toggled().connect([this, uid_capture] {
        TaskDao d(app_.db());
        auto current = d.get_by_uid(uid_capture);
        if (!current.has_value())
            return;

        try {
            app_.tasks().toggle_complete(*current);
            g_idle_add(
                [](gpointer p) -> gboolean {
                    static_cast<TasksView*>(p)->rebuild();
                    return G_SOURCE_REMOVE;
                },
                this);
        }
        catch (std::exception const& err) {
            Gtk::Window* transient = dynamic_cast<Gtk::Window*>(get_toplevel());
            if (transient) {
                Gtk::MessageDialog dlg(*transient, err.what(), false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
                dlg.run();
            }
            else std::fprintf(stderr, "toggle task: %s\n", err.what());
            g_idle_add(
                [](gpointer p) -> gboolean {
                    static_cast<TasksView*>(p)->rebuild();
                    return G_SOURCE_REMOVE;
                },
                this);
        }
    });

    row->add(*hb);
    return row;
}

void TasksView::rebuild()
{
    for (Gtk::Widget* rw : list_.get_children()) list_.remove(*rw);

    TaskDao dao(app_.db());
    auto const all_tasks = dao.get_all();

    std::multimap<std::string, CalDavTask const*> children{};
    std::vector<CalDavTask const*> roots{};
    for (auto const& t : all_tasks) {
        if (t.parent_uid && !t.parent_uid->empty())
            children.emplace(*t.parent_uid, &t);
        else roots.push_back(&t);
    }
    std::stable_sort(roots.begin(), roots.end(), [](CalDavTask const* a, CalDavTask const* b) {
        return a->summary < b->summary;
    });

    std::function<void(CalDavTask const&, int)> visit = [&](CalDavTask const& t, int depth) {
        list_.append(*make_row(t, depth));

        auto range = children.equal_range(t.uid);

        std::vector<CalDavTask const*> kids;
        kids.reserve(static_cast<std::size_t>(std::distance(range.first, range.second)));
        for (auto it = range.first; it != range.second; ++it)
            kids.push_back(it->second);

        std::stable_sort(kids.begin(), kids.end(),
            [](CalDavTask const* a, CalDavTask const* b) { return a->summary < b->summary; });

        for (auto* k : kids) visit(*k, depth + 1);
    };

    for (auto const* rt : roots) visit(*rt, 0);
    list_.show_all();
}

void TasksView::on_quick_submit(Glib::ustring const& text)
{
    AppSettings settings = merged_app_preferences(app_.prefs());
    ParsedTask parsed = TaskInputParser::parse(text.raw(), settings.task_defaults);

    Gtk::Window* transient = dynamic_cast<Gtk::Window*>(get_toplevel());

    auto show_warning = [&](Glib::ustring const& msg) {
        if (transient) {
            Gtk::MessageDialog dlg(*transient, msg, false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_CLOSE);
            dlg.run();
        }
        else std::fprintf(stderr, "%s\n", msg.raw().c_str());
    };

    std::optional<std::string> cal = app_.secrets().get(CredentialKey::CALDAV_DEFAULT_TASK_CALENDAR);
    if (!cal.has_value() || cal->empty()) {
        show_warning("Set CALDAV_DEFAULT_TASK_CALENDAR credential to your tasks collection URL.");
        return;
    }

    CalDavTask t{};
    EpochMillis ms = millis_now_wall();
    t.summary = parsed.summary;
    t.priority = parsed.priority;
    t.categories = parsed.categories;
    t.due = parsed.due;
    t.status = TaskStatus::NeedsAction;
    t.percent_complete = 0;
    t.created = ms;
    t.last_modified = ms;

    try {
        app_.tasks().create(t, *cal);
        rebuild();
    }
    catch (std::exception const& err) {
        if (transient) {
            Gtk::MessageDialog dlg(*transient, err.what(), false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
            dlg.run();
        }
        else std::fprintf(stderr, "%s\n", err.what());
    }
}

} // namespace cd
