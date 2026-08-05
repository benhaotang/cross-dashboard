#pragma once

#include "domain/models.h"

#include <gtkmm/flowbox.h>

#include <string>
#include <vector>

namespace cd {

enum class TagKind { Neutral, Magic, Time };

TagKind classify_tag(std::string const& tag, std::vector<std::string> const& magic_tags);

/** Creates a compact, wrapping row of read-only #tag pills. */
Gtk::FlowBox* make_tag_flow(
    std::vector<std::string> tags, std::vector<std::string> const& magic_tags = {});

/** Configured Kanban columns plus the fixed Covey planning tags. */
std::vector<std::string> planning_magic_tags(AppSettings const& settings);

} // namespace cd
