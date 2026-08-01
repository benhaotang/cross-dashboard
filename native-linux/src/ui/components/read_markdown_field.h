#pragma once

#include <gtkmm/box.h>
#include <gtkmm/label.h>

#include <string>

#include "markdown_view.h"

namespace cd {

/** Label (small caps via markup) + read-only markdown body — mirrors mobile `ReadMarkdownField`. */
class ReadMarkdownField final : public Gtk::Box {
public:
    explicit ReadMarkdownField(MarkdownView::HeightMode height_mode = MarkdownView::HeightMode::Full);
    ReadMarkdownField(ReadMarkdownField const&) = delete;
    ReadMarkdownField& operator=(ReadMarkdownField const&) = delete;

    void set_field_label(std::string const& small_caps_title);
    void set_markdown(std::string const& markdown);

private:
    Gtk::Label title_;
    MarkdownView body_;
};

} // namespace cd
