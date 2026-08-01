#include "create_event_from_memo_dialog.h"

#include "app_container.h"
#include "data/prefs/prefs.h"

#include <chrono>

namespace cd {

namespace {

EpochMillis now_ms()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string first_calendar_href(AppContainer& app)
{
    if (auto href = app.secrets().get(CredentialKey::CALDAV_DEFAULT_EVENT_CALENDAR))
        if (!href->empty()) return *href;
    auto hrefs = app.events().selected_calendar_hrefs();
    return hrefs.empty() ? std::string{} : hrefs.front();
}

} // namespace

CreateEventFromMemoDialog::CreateEventFromMemoDialog(
    Gtk::Window& parent, AppContainer& app, std::string memo_content)
    : Gtk::Dialog("Create event from memo", parent, true)
    , app_(app)
{
    add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    add_button("_Create", Gtk::RESPONSE_OK);

    title_entry_.set_placeholder_text("Event title");
    title_entry_.set_text(memo_content.substr(0, std::min<std::size_t>(memo_content.size(), 80)));
    start_entry_.set_placeholder_text("Start in minutes from now (default 60)");
    start_entry_.set_text("60");
    end_entry_.set_placeholder_text("Duration in minutes (default 30)");
    end_entry_.set_text("30");

    Gtk::Box& box = *get_content_area();
    box.set_spacing(8);
    box.pack_start(title_entry_, false, false);
    box.pack_start(start_entry_, false, false);
    box.pack_start(end_entry_, false, false);

    show_all_children();
}

bool CreateEventFromMemoDialog::create_event()
{
    std::string cal = first_calendar_href(app_);
    if (cal.empty() || title_entry_.get_text().empty())
        return false;

    int start_in = std::max(1, std::stoi(start_entry_.get_text().empty() ? "60" : start_entry_.get_text()));
    int duration = std::max(5, std::stoi(end_entry_.get_text().empty() ? "30" : end_entry_.get_text()));

    EpochMillis start = now_ms() + static_cast<EpochMillis>(start_in) * 60 * 1000;
    EpochMillis end = start + static_cast<EpochMillis>(duration) * 60 * 1000;

    CalendarEvent e{};
    e.summary = title_entry_.get_text();
    e.start = start;
    e.end = end;
    app_.events().create(e, cal);
    return true;
}

} // namespace cd
