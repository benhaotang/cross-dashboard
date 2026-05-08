#pragma once

#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>

namespace cd {

class AppContainer;

class CreateEventFromMemoDialog final : public Gtk::Dialog {
public:
    CreateEventFromMemoDialog(Gtk::Window& parent, AppContainer& app, std::string memo_content);
    bool create_event();

private:
    AppContainer& app_;
    Gtk::Entry title_entry_;
    Gtk::Entry start_entry_;
    Gtk::Entry end_entry_;
};

} // namespace cd
