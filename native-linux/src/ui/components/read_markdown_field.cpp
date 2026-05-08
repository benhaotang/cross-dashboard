#include "read_markdown_field.h"

#include "markdown_view.h"

#include <glib.h>

namespace cd {

ReadMarkdownField::ReadMarkdownField()
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 6)
    , title_("")
    , body_{}
{
    title_.set_halign(Gtk::ALIGN_START);
    title_.set_use_markup(true);
    pack_start(title_, false, false);
    pack_start(body_, false, false);
}

void ReadMarkdownField::set_field_label(std::string const& small_caps_title)
{
    if (small_caps_title.empty()) {
        title_.hide();
        return;
    }
    title_.show();
    gchar* esc = g_markup_escape_text(small_caps_title.c_str(), -1);
    Glib::ustring inner = esc ? esc : "";
    if (esc) g_free(esc);
    title_.set_markup("<span size='smaller' weight='bold' letter_spacing='1000'>" + inner + "</span>");
}

void ReadMarkdownField::set_markdown(std::string const& markdown)
{
    body_.load_markdown(markdown);
}

} // namespace cd
