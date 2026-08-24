#include "save_to_karakeep_dialog.h"

#include "app_container.h"

#include <gtkmm/box.h>
#include <gtkmm/messagedialog.h>

#include <map>
#include <set>

#include <atk/atk.h>
#include <gtk/gtk.h>

namespace cd {

SaveToKarakeepDialog::SaveToKarakeepDialog(
    Gtk::Window& parent, AppContainer& app, std::vector<std::string> urls)
    : Gtk::Dialog(urls.size() == 1 ? "Save link to Karakeep" : "Save links to Karakeep", parent, true)
    , app_(app)
    , urls_(std::move(urls))
{
    add_button("Cancel", Gtk::RESPONSE_CANCEL);
    add_button("Save", Gtk::RESPONSE_OK);
    set_default_response(Gtk::RESPONSE_OK);
    set_default_size(440, -1);

    auto* content = get_content_area();
    content->set_spacing(10);
    content->set_margin_start(16);
    content->set_margin_end(16);
    content->set_margin_top(12);
    content->set_margin_bottom(12);

    auto* count = Gtk::manage(new Gtk::Label(
        urls_.size() == 1 ? urls_.front() : std::to_string(urls_.size()) + " links"));
    count->set_halign(Gtk::ALIGN_START);
    count->set_ellipsize(Pango::ELLIPSIZE_MIDDLE);
    content->pack_start(*count, false, false);

    folder_.append("", "No folder");
    folder_.set_active_id("");
    folder_.set_hexpand(true);
    if (AtkObject* accessible = gtk_widget_get_accessible(GTK_WIDGET(folder_.gobj())))
        atk_object_set_name(accessible, "Karakeep folder");

    try {
        auto folders = app_.karakeep().list_folders();
        std::map<std::string, KarakeepFolder const*> by_id;
        for (auto const& folder : folders) by_id.emplace(folder.id, &folder);                        for (auto const& folder : folders) {
            std::vector<std::string> names{folder.name};                                                 std::set<std::string> visited{folder.id};
            auto parent_id = folder.parent_id;                                                           while (parent_id.has_value() && visited.insert(*parent_id).second) {                             auto parent = by_id.find(*parent_id);                                                        if (parent == by_id.end()) break;                                                            names.insert(names.begin(), parent->second->name);
                parent_id = parent->second->parent_id;
            }
            std::string label;
            for (auto const& name : names) label += (label.empty() ? "" : " / ") + name;
            folder_.append(folder.id, label);
        }
    }                                                                                            catch (std::exception const& error) {
        status_.set_text(error.what());
        gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(status_.gobj())), "error");
    }
    content->pack_start(folder_, false, false);
    status_.set_halign(Gtk::ALIGN_START);
    content->pack_start(status_, false, false);
    show_all_children();
}

void SaveToKarakeepDialog::save()
{
    auto id = folder_.get_active_id();
    std::optional<std::string> folder_id = id.empty() ? std::nullopt : std::optional<std::string>{id};
    app_.karakeep().save_urls(urls_, folder_id);
}

} // namespace cd
