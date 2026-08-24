#pragma once

#include <gtkmm/comboboxtext.h>
#include <gtkmm/dialog.h>
#include <gtkmm/label.h>

#include <string>
#include <vector>

namespace cd {

class AppContainer;

class SaveToKarakeepDialog final : public Gtk::Dialog {
public:
    SaveToKarakeepDialog(Gtk::Window& parent, AppContainer& app, std::vector<std::string> urls);
    void save();

private:
    AppContainer& app_;
    std::vector<std::string> urls_;
    Gtk::ComboBoxText folder_;
    Gtk::Label status_;
};

} // namespace cd
