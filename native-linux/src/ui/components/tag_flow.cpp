#include "tag_flow.h"

#include <gtk/gtk.h>
#include <gtkmm/flowboxchild.h>
#include <gtkmm/label.h>

#include <algorithm>
#include <cctype>
#include <regex>

namespace cd {

namespace {

std::string normalized_tag(std::string tag)
{
    if (!tag.empty() && tag.front() == '#') tag.erase(tag.begin());
    std::transform(tag.begin(), tag.end(), tag.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return tag;
}

bool is_time_tag(std::string const& tag)
{
    static std::regex const duration(R"(^\d+(m|h)$)", std::regex::icase);
    return std::regex_match(normalized_tag(tag), duration);
}

bool is_magic_tag(std::string const& tag, std::vector<std::string> const& magic_tags)
{
    auto const normalized = normalized_tag(tag);
    return std::any_of(magic_tags.begin(), magic_tags.end(), [&normalized](std::string const& magic) {
        return normalized_tag(magic) == normalized;
    });
}

} // namespace

TagKind classify_tag(std::string const& tag, std::vector<std::string> const& magic_tags)
{
    if (is_time_tag(tag)) return TagKind::Time;
    if (is_magic_tag(tag, magic_tags)) return TagKind::Magic;
    return TagKind::Neutral;
}

std::vector<std::string> planning_magic_tags(AppSettings const& settings)
{
    std::vector<std::string> tags = settings.kanban_columns;
    tags.insert(tags.end(), kCoveyQuadrantTags.begin(), kCoveyQuadrantTags.end());
    return tags;
}

Gtk::FlowBox* make_tag_flow(
    std::vector<std::string> tags, std::vector<std::string> const& magic_tags)
{
    std::sort(tags.begin(), tags.end());
    tags.erase(std::unique(tags.begin(), tags.end()), tags.end());

    auto* flow = Gtk::manage(new Gtk::FlowBox());
    flow->set_selection_mode(Gtk::SELECTION_NONE);
    flow->set_halign(Gtk::ALIGN_START);
    flow->set_hexpand(true);
    flow->set_max_children_per_line(12);
    flow->set_row_spacing(4);
    flow->set_column_spacing(4);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(GTK_WIDGET(flow->gobj())), "cd-tag-flow");

    for (auto const& tag : tags) {
        if (tag.empty()) continue;
        auto* child = Gtk::manage(new Gtk::FlowBoxChild());
        std::string const display = tag.front() == '#' ? tag : "#" + tag;
        auto* label = Gtk::manage(new Gtk::Label(display));
        auto* context = gtk_widget_get_style_context(GTK_WIDGET(label->gobj()));
        gtk_style_context_add_class(context, "cd-tag-chip");
        if (classify_tag(tag, magic_tags) == TagKind::Time)
            gtk_style_context_add_class(context, "cd-tag-chip-time");
        else if (classify_tag(tag, magic_tags) == TagKind::Magic)
            gtk_style_context_add_class(context, "cd-tag-chip-magic");
        else
            gtk_style_context_add_class(context, "cd-tag-chip-neutral");
        child->add(*label);
        flow->add(*child);
    }
    return flow;
}

} // namespace cd
