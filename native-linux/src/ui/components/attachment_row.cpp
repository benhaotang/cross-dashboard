#include "attachment_row.h"

#include <glib.h>
#include <gtk/gtk.h>

#include <cstdio>
#include <sstream>

#include <gtkmm/image.h>
#include <gtkmm/label.h>

namespace cd {

AttachmentRow::AttachmentRow(GiteaAttachment const& a)
    : Gtk::Button{}
    , att_(a)
{
    set_relief(Gtk::RELIEF_NONE);
    set_halign(Gtk::ALIGN_FILL);

    auto* hb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 8));
    auto* ic = Gtk::manage(new Gtk::Image());
    ic->set_from_icon_name("mail-attachment-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
    auto* lab = Gtk::manage(new Gtk::Label{});

    std::ostringstream sz;
    if (a.size >= 1024 * 1024)
        sz << (a.size / (1024 * 1024)) << " MiB";
    else if (a.size >= 1024)
        sz << (a.size / 1024) << " KiB";
    else
        sz << a.size << " B";

    gchar* esc = g_markup_escape_text(a.name.c_str(), -1);
    Glib::ustring line = esc ? esc : "";
    if (esc) g_free(esc);
    line += " · " + sz.str();
    lab->set_markup("<small>" + line + "</small>");
    lab->set_halign(Gtk::ALIGN_START);
    lab->set_line_wrap(true);

    hb->pack_start(*ic, false, false);
    hb->pack_start(*lab, true, true);

    add(*hb);

    signal_clicked().connect([this] {
        GError* err{};
        GtkWidget* top = gtk_widget_get_toplevel(GTK_WIDGET(gobj()));
        if (GTK_IS_WINDOW(top))
            gtk_show_uri_on_window(
                GTK_WINDOW(top), att_.download_url.c_str(), GDK_CURRENT_TIME, &err);
        else {
            if (!gtk_show_uri_on_window(nullptr, att_.download_url.c_str(), GDK_CURRENT_TIME, &err))
                std::fprintf(stderr, "open url: %s\n", err ? err->message : "?");
        }
        if (err) g_error_free(err);
    });

    atk_object_set_name(gtk_widget_get_accessible(GTK_WIDGET(gobj())), "Issue attachment link");
}

} // namespace cd
