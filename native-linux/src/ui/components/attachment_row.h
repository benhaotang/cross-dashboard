#pragma once

#include "domain/models.h"

#include <gtkmm/box.h>
#include <gtkmm/button.h>

namespace cd {

/** Paperclip-style row: open `download_url` in the default browser / handler. */
class AttachmentRow final : public Gtk::Button {
public:
    explicit AttachmentRow(GiteaAttachment const&);

    AttachmentRow(AttachmentRow const&) = delete;
    AttachmentRow& operator=(AttachmentRow const&) = delete;

private:
    GiteaAttachment const att_;
};

} // namespace cd
