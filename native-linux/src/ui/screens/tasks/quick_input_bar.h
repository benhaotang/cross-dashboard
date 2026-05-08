#pragma once

#include <glibmm/ustring.h>
#include <sigc++/signal.h>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/entry.h>

namespace cd {

/** GtkEntry + send — mirrors Compose quick-input bar. */
class QuickInputBar final : public Gtk::Box {
public:
    QuickInputBar();

    void grab_entry_focus();

    sigc::signal<void(Glib::ustring const&)> signal_submit_requested;

private:
    void on_submit_clicked();
    Gtk::Entry entry_;
    Gtk::Button send_;
};

} // namespace cd
