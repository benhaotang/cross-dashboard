#pragma once

#include "domain/models.h"

#include <gtkmm/checkbutton.h>
#include <gtkmm/dialog.h>
#include <gtkmm/label.h>

#include <vector>

namespace cd {

class AppContainer;

class ExtractTasksDialog final : public Gtk::Dialog {
public:
    ExtractTasksDialog(Gtk::Window& parent, AppContainer& app, std::string memo_content);

    /** Creates tasks from checked lines. Returns created count. */
    int create_checked_tasks();

private:
    AppContainer& app_;
    std::vector<std::string> lines_;
    std::vector<Gtk::CheckButton*> checks_;
};

} // namespace cd
