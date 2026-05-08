#pragma once

#include <gtkmm/box.h>

namespace cd {

/** Placeholder Phase 2 stub — Phase 3 lists use inline panel / dialog branching. */
class PropertyPanel final : public Gtk::Box {
public:
    PropertyPanel() : Gtk::Box(Gtk::ORIENTATION_VERTICAL) {}

    template <typename T>
    void show_item(T const&) {
        /* no-op */
    }
};

} // namespace cd
