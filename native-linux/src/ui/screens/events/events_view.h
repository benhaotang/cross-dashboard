#pragma once

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/paned.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/scrolledwindow.h>

#include "domain/models.h"

#include <vector>

namespace cd {

class AppContainer;
class MarkdownView;
class SyncScheduler;

/** Day / week / month filtered calendar list + detail (Phase 3). */
class EventsView final : public Gtk::Box {
public:
    enum class TimeFilter : std::uint8_t { Day, Week, Month };

    EventsView(AppContainer&, SyncScheduler&);

    void refresh();
    bool reveal_event(std::string const& uid);

private:
    void rebuild();
    void on_filter_changed();
    void on_selection_changed();

    [[nodiscard]] static GdkRGBA color_for_calendar(std::optional<std::string> const& calendar_href);
    static void range_for_filter(TimeFilter, EpochMillis* out_begin, EpochMillis* out_end);

    AppContainer& app_;
    SyncScheduler& sync_;
    Gtk::Paned paned_;
    Gtk::Box filter_box_;
    Gtk::Button refresh_btn_{};
    Gtk::RadioButton::Group rb_group_;
    Gtk::RadioButton rb_day_;
    Gtk::RadioButton rb_week_;
    Gtk::RadioButton rb_month_;
    Gtk::ScrolledWindow scroll_;
    Gtk::ListBox list_;
    Gtk::ScrolledWindow detail_scroll_;
    Gtk::Box detail_;
    Gtk::Label detail_title_;
    Gtk::Label detail_when_;
    MarkdownView* detail_body_{};

    TimeFilter filter_{TimeFilter::Day};
    std::vector<CalendarEvent> filtered_;
};

} // namespace cd
