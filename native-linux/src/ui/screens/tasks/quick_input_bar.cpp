#include "quick_input_bar.h"

#include <gtk/gtk.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace cd {

namespace {

void trim_inplace(std::string& s)
{
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
}

} // namespace

QuickInputBar::QuickInputBar()
    : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 6)
{
    entry_.set_hexpand(true);
    entry_.set_placeholder_text("Task … #tag !!! priority");
    entry_.signal_activate().connect(sigc::mem_fun(*this, &QuickInputBar::on_submit_clicked));

    send_.set_label("_Add");
    send_.set_use_underline(false);
    send_.signal_clicked().connect(sigc::mem_fun(*this, &QuickInputBar::on_submit_clicked));

    pack_start(entry_, true, true);
    pack_start(send_, false, false);

    atk_object_set_name(gtk_widget_get_accessible(GTK_WIDGET(entry_.gobj())), "Task quick input");

    atk_object_set_name(gtk_widget_get_accessible(GTK_WIDGET(send_.gobj())), "Add task");
}

void QuickInputBar::grab_entry_focus()
{
    entry_.grab_focus();
}

void QuickInputBar::on_submit_clicked()
{
    std::string raw = entry_.get_text().raw();
    trim_inplace(raw);
    if (!raw.empty())
        signal_submit_requested.emit(Glib::ustring(raw));
    entry_.set_text({});
}

} // namespace cd
