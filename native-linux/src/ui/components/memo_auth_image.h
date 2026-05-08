#pragma once

#include <gtkmm/eventbox.h>
#include <gtkmm/image.h>

#include <string>

namespace cd {

class AppContainer;

/** Authenticated thumbnail preview for a memo attachment. */
class MemoAuthImage final : public Gtk::EventBox {
public:
    MemoAuthImage(AppContainer& app, std::string attachment_name, std::string filename);

private:
    void load_thumbnail();
    void on_click_open();

    AppContainer& app_;
    std::string attachment_name_;
    std::string filename_;
    std::string full_url_;
    Gtk::Image image_;
};

} // namespace cd
