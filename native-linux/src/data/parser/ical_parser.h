#pragma once

#include "domain/models.h"

#include <optional>
#include <string>
#include <vector>

namespace cd {

/** Hand-written iCalendar RFC 5545 — mirrors Kotlin `ICalParser`. */
struct ICalParser {
    [[nodiscard]] static std::vector<CalendarEvent> parse_events(std::string const& text,
        std::optional<std::string> calendar_href = std::nullopt,
        std::optional<std::string> resource_href = std::nullopt,
        std::optional<std::string> resource_etag = std::nullopt);

    [[nodiscard]] static std::vector<CalDavTask> parse_tasks(std::string const& text,
        std::optional<std::string> calendar_href = std::nullopt,
        std::optional<std::string> resource_href = std::nullopt,
        std::optional<std::string> resource_etag = std::nullopt);

    [[nodiscard]] static std::vector<Note> parse_notes(std::string const& text,
        std::optional<std::string> calendar_href = std::nullopt,
        std::optional<std::string> resource_href = std::nullopt,
        std::optional<std::string> resource_etag = std::nullopt);

    [[nodiscard]] static std::string serialize_task(CalDavTask const& task);
    [[nodiscard]] static std::string serialize_note(Note const& note);
    [[nodiscard]] static std::string serialize_event(CalendarEvent const& ev);
};

} // namespace cd
