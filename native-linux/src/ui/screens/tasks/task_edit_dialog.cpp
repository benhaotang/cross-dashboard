#include "task_edit_dialog.h"

#include "components/tag_flow.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <sstream>

#include <glib.h>

#include <gtkmm/grid.h>
#include <gtkmm/label.h>

namespace cd {

namespace {

EpochMillis millis_now_wall()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

Gtk::Label* align_start_label(char const* text)
{
    auto* l = Gtk::manage(new Gtk::Label(text));
    l->set_halign(Gtk::ALIGN_START);
    return l;
}

void trim_inplace(std::string& s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
}

std::string join_categories(std::vector<std::string> const& cats)
{
    std::ostringstream o;
    for (std::size_t i = 0; i < cats.size(); ++i) {
        if (i) o << ", ";
        o << cats[i];
    }
    return o.str();
}

std::vector<std::string> split_categories(std::string raw)
{
    trim_inplace(raw);
    std::vector<std::string> out;
    std::size_t pos = 0;
    while (pos < raw.size()) {
        std::size_t const comma = raw.find(',', pos);
        std::string part = comma == std::string::npos ? raw.substr(pos) : raw.substr(pos, comma - pos);
        trim_inplace(part);
        if (!part.empty()) out.push_back(std::move(part));
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    return out;
}

std::optional<EpochMillis> parse_due_local(std::string const& raw)
{
    std::string s = raw;
    trim_inplace(s);
    if (s.empty()) return std::nullopt;
    int y{};
    int mo{};
    int d{};
    int h{};
    int mi{};
    if (std::sscanf(s.c_str(), "%d-%d-%d %d:%d", &y, &mo, &d, &h, &mi) != 5)
        return std::nullopt;
    GDateTime* dt = g_date_time_new_local(y, mo, d, h, mi, 0.0);
    if (!dt) return std::nullopt;
    gint64 const secs = g_date_time_to_unix(dt);
    g_date_time_unref(dt);
    return static_cast<EpochMillis>(secs * 1000LL);
}

std::string format_due_local(EpochMillis ms)
{
    GDateTime* dt = g_date_time_new_from_unix_local(static_cast<gint64>(ms / 1000));
    if (!dt) return {};
    gchar* s = g_date_time_format(dt, "%Y-%m-%d %H:%M");
    g_date_time_unref(dt);
    if (!s) return {};
    std::string out{s};
    g_free(s);
    return out;
}

int status_row(TaskStatus s)
{
    switch (s) {
    case TaskStatus::NeedsAction: return 0;
    case TaskStatus::InProcess: return 1;
    case TaskStatus::Completed: return 2;
    case TaskStatus::Cancelled: return 3;
    }
    return 0;
}

TaskStatus status_from_row(int row)
{
    switch (row) {
    case 1: return TaskStatus::InProcess;
    case 2: return TaskStatus::Completed;
    case 3: return TaskStatus::Cancelled;
    default: return TaskStatus::NeedsAction;
    }
}

int priority_row(int p)
{
    if (p == 1) return 1;
    if (p == 5) return 2;
    if (p == 9) return 3;
    return 0;
}

int priority_from_row(int row)
{
    switch (row) {
    case 1: return 1;
    case 2: return 5;
    case 3: return 9;
    default: return 0;
    }
}

} // namespace

TaskEditDialog::TaskEditDialog(Gtk::Window& parent, CalDavTask const& task)
    : Gtk::Dialog("Edit task", parent, true)
    , base_(task)
{
    add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    add_button("_Save", Gtk::RESPONSE_OK);

    summary_.set_hexpand(true);
    summary_.set_text(task.summary);
    if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(summary_.gobj())))
        atk_object_set_name(a, "Task summary");

    description_.set_wrap_mode(Gtk::WRAP_WORD_CHAR);
    description_.get_buffer()->set_text(task.description.value_or(""));

    status_.append("To do");
    status_.append("In progress");
    status_.append("Completed");
    status_.append("Cancelled");
    status_.set_active(status_row(task.status));

    priority_.append("None");
    priority_.append("High");
    priority_.append("Medium");
    priority_.append("Low");
    priority_.set_active(priority_row(task.priority));

    due_toggle_.set_active(task.due.has_value());
    if (task.due) {
        due_entry_.set_text(format_due_local(*task.due));
    }
    due_entry_.set_placeholder_text("YYYY-MM-DD HH:MM");
    due_entry_.set_sensitive(task.due.has_value());

    due_toggle_.signal_toggled().connect([this] { due_entry_.set_sensitive(due_toggle_.get_active()); });

    categories_.set_text(join_categories(task.categories));
    categories_.set_placeholder_text("e.g. work, errands");

    estimate_unit_.append("Minutes");
    estimate_unit_.append("Hours");
    std::vector<std::string> non_time_categories;
    for (auto const& category : task.categories) {
        if (classify_tag(category, {}) != TagKind::Time) {
            non_time_categories.push_back(category);
            continue;
        }
        std::string normalized = category;
        if (!normalized.empty() && normalized.front() == '#') normalized.erase(normalized.begin());
        if (!normalized.empty()) {
            estimate_unit_.set_active_text(
                normalized.back() == 'h' || normalized.back() == 'H' ? "Hours" : "Minutes");
            normalized.pop_back();
            estimate_amount_.set_text(normalized);
        }
    }
    categories_.set_text(join_categories(non_time_categories));
    estimate_amount_.set_placeholder_text("30");
    estimate_amount_.set_width_chars(8);
    if (estimate_unit_.get_active_row_number() < 0) estimate_unit_.set_active(0);
    if (AtkObject* a = gtk_widget_get_accessible(GTK_WIDGET(estimate_amount_.gobj())))
        atk_object_set_name(a, "Task time estimate amount");

    auto* desc_scroll = Gtk::manage(new Gtk::ScrolledWindow());
    desc_scroll->set_min_content_height(120);
    desc_scroll->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    desc_scroll->add(description_);

    // The content area owns this grid.  A stack-local Grid is destroyed when
    // the constructor returns, leaving the still-open dialog with an empty
    // body.
    auto* grid = Gtk::manage(new Gtk::Grid());
    grid->set_column_spacing(10);
    grid->set_row_spacing(8);

    int r = 0;
    grid->attach(*align_start_label("Summary"), 0, r, 1, 1);
    grid->attach(summary_, 1, r++, 1, 1);
    grid->attach(*align_start_label("Status"), 0, r, 1, 1);
    grid->attach(status_, 1, r++, 1, 1);
    grid->attach(*align_start_label("Priority"), 0, r, 1, 1);
    grid->attach(priority_, 1, r++, 1, 1);
    grid->attach(*align_start_label("Tags"), 0, r, 1, 1);
    grid->attach(categories_, 1, r++, 1, 1);
    auto* estimate_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 6));
    estimate_box->pack_start(estimate_amount_, false, false);
    estimate_box->pack_start(estimate_unit_, false, false);
    grid->attach(*align_start_label("Time estimate"), 0, r, 1, 1);
    grid->attach(*estimate_box, 1, r++, 1, 1);
    grid->attach(due_toggle_, 1, r++, 1, 1);
    grid->attach(*align_start_label("Due (local)"), 0, r, 1, 1);
    grid->attach(due_entry_, 1, r++, 1, 1);
    grid->attach(*align_start_label("Description"), 0, r, 1, 1);
    grid->attach(*desc_scroll, 1, r++, 1, 1);

    Gtk::Box& box = *get_content_area();
    box.set_spacing(10);
    box.set_border_width(10);
    box.pack_start(*grid, true, true);

    set_default_size(520, 460);
    show_all_children();
    summary_.grab_focus();
}

std::optional<CalDavTask> TaskEditDialog::result_if_ok()
{
    Glib::ustring const sum = summary_.get_text();
    std::string const sum_raw = sum.raw();
    std::string trimmed = sum_raw;
    trim_inplace(trimmed);
    if (trimmed.empty())
        return std::nullopt;

    CalDavTask out = base_;
    out.summary = trimmed;
    Glib::RefPtr<Gtk::TextBuffer> buf = description_.get_buffer();
    if (buf) {
        Glib::ustring body = buf->get_text();
        std::string const br = body.raw();
        if (br.empty()) out.description = std::nullopt;
        else out.description = br;
    }

    int const st = status_.get_active_row_number();
    int const pr = priority_.get_active_row_number();
    out.status = status_from_row(st < 0 ? 0 : st);
    out.priority = priority_from_row(pr < 0 ? 0 : pr);
    out.categories = split_categories(categories_.get_text());
    out.categories.erase(
        std::remove_if(out.categories.begin(), out.categories.end(), [](std::string const& category) {
            return classify_tag(category, {}) == TagKind::Time;
        }),
        out.categories.end());
    std::string amount = estimate_amount_.get_text();
    trim_inplace(amount);
    try {
        bool const numeric = !amount.empty() && std::all_of(amount.begin(), amount.end(), [](unsigned char c) {
            return std::isdigit(c) != 0;
        });
        int const value = numeric ? std::stoi(amount) : 0;
        if (value > 0) {
            std::string const unit = estimate_unit_.get_active_row_number() == 1 ? "h" : "m";
            out.categories.push_back(std::to_string(value) + unit);
        }
    }
    catch (...) {
        // Invalid or empty estimates are treated as no estimate.
    }

    if (due_toggle_.get_active()) {
        std::string due_s = due_entry_.get_text();
        if (auto parsed = parse_due_local(due_s)) out.due = parsed;
        else out.due = std::nullopt;
    }
    else {
        out.due = std::nullopt;
    }

    if (out.status == TaskStatus::Completed) {
        out.percent_complete = 100;
        if (!out.completed) out.completed = millis_now_wall(); // pattern from toggle_complete
    }
    else {
        if (base_.status == TaskStatus::Completed && out.status != TaskStatus::Completed) {
            out.completed = std::nullopt;
            out.percent_complete = 0;
        }
    }

    return out;
}

} // namespace cd
