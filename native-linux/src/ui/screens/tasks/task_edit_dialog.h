#pragma once

#include "domain/models.h"

#include <gtkmm/checkbutton.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textview.h>

namespace cd {

/** Edit-task form (parity with macOS TaskEditForm / Android task edit). */
class TaskEditDialog final : public Gtk::Dialog {
public:
    TaskEditDialog(Gtk::Window& parent, CalDavTask const& task);

    /** If the form is valid, returns updated task (caller should set last_modified). */
    [[nodiscard]] std::optional<CalDavTask> result_if_ok();

private:
    CalDavTask base_{};
    Gtk::Entry summary_{};
    Gtk::TextView description_{};
    Gtk::ComboBoxText status_{};
    Gtk::ComboBoxText priority_{};
    Gtk::CheckButton due_toggle_{"Has due date"};
    Gtk::Entry due_entry_{};
    Gtk::Entry categories_{};
};

} // namespace cd
