#include "extract_tasks_dialog.h"

#include "app_container.h"
#include "data/parser/task_input_parser.h"
#include "data/prefs/prefs.h"

#include <sstream>

namespace cd {

namespace {

std::vector<std::string> extract_unchecked_lines(std::string const& content)
{
    std::vector<std::string> out;
    std::istringstream in(content);
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("- [ ] ", 0) == 0) {
            out.push_back(line.substr(6));
            continue;
        }
        if (line.rfind("* [ ] ", 0) == 0)
            out.push_back(line.substr(6));
    }
    return out;
}

std::string first_calendar_href(AppContainer& app)
{
    auto hrefs = app.events().selected_calendar_hrefs();
    return hrefs.empty() ? std::string{} : hrefs.front();
}

} // namespace

ExtractTasksDialog::ExtractTasksDialog(Gtk::Window& parent, AppContainer& app, std::string memo_content)
    : Gtk::Dialog("Extract tasks", parent, true)
    , app_(app)
    , lines_(extract_unchecked_lines(memo_content))
{
    add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    add_button("_Create", Gtk::RESPONSE_OK);

    Gtk::Box& box = *get_content_area();
    box.set_spacing(6);
    if (lines_.empty()) {
        auto* label = Gtk::manage(new Gtk::Label("No unchecked checklist items found."));
        label->set_halign(Gtk::ALIGN_START);
        box.pack_start(*label, false, false);
    }
    else {
        for (std::string const& line : lines_) {
            auto* cb = Gtk::manage(new Gtk::CheckButton(line));
            cb->set_active(true);
            checks_.push_back(cb);
            box.pack_start(*cb, false, false);
        }
    }

    set_default_size(520, 320);
    show_all_children();
}

int ExtractTasksDialog::create_checked_tasks()
{
    std::string cal = first_calendar_href(app_);
    if (cal.empty())
        return 0;

    AppSettings const settings = merged_app_preferences(app_.prefs());
    int created = 0;
    for (std::size_t i = 0; i < checks_.size(); ++i) {
        if (!checks_[i]->get_active())
            continue;
        ParsedTask parsed = TaskInputParser::parse(lines_[i], settings.task_defaults);
        if (parsed.summary.empty())
            continue;

        CalDavTask t{};
        t.summary = parsed.summary;
        t.priority = parsed.priority;
        t.categories = parsed.categories;
        t.due = parsed.due;
        app_.tasks().create(t, cal);
        ++created;
    }
    return created;
}

} // namespace cd
