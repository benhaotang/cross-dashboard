#pragma once

#include "domain/models.h"

#include <gtkmm/button.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/dialog.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textview.h>

#include <optional>
#include <vector>

namespace cd {

class AppContainer;

class CreateMemoDialog final : public Gtk::Dialog {
public:
    CreateMemoDialog(Gtk::Window& parent, AppContainer& app, std::string initial_text = {});
    CreateMemoDialog(Gtk::Window& parent, AppContainer& app, MemosMemo const& existing);

    [[nodiscard]] std::string content();
    [[nodiscard]] MemoVisibility visibility() const;
    [[nodiscard]] std::vector<PendingAttachment> attachments() const { return attachments_; }

private:
    void on_add_attachment();

    AppContainer& app_;
    Gtk::TextView content_view_;
    Gtk::ComboBoxText visibility_combo_;
    Gtk::Button add_attachment_btn_{"Add attachment..."};
    Gtk::Label attachments_label_;
    std::vector<PendingAttachment> attachments_;
    bool edit_mode_{};
    std::string editing_name_;
};

} // namespace cd
