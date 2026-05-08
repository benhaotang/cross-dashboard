#include "memo_auth_image.h"

#include "app_container.h"
#include "data/network/http_soup.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

namespace cd {

MemoAuthImage::MemoAuthImage(AppContainer& app, std::string attachment_name, std::string filename)
    : app_(app)
    , attachment_name_(std::move(attachment_name))
    , filename_(std::move(filename))
{
    add(image_);
    set_size_request(160, 160);
    set_visible_window(false);
    signal_button_press_event().connect([this](GdkEventButton*) {
        on_click_open();
        return true;
    });
    load_thumbnail();
    show_all_children();
}

void MemoAuthImage::load_thumbnail()
{
    auto base = app_.memos_client().base_url_opt();
    auto token = app_.secrets().get(CredentialKey::MEMOS_TOKEN);
    if (!base.has_value() || !token.has_value())
        return;

    // Keep Memos attachment name as-is: it already includes `attachments/...`.
    full_url_ = *base + "/file/" + attachment_name_ + "/" + filename_;
    auto [status, payload] = soup_sync_request(app_.soup_session(), "GET", full_url_, std::nullopt, nullptr,
        {{"Authorization", "Bearer " + *token}});
    if (status < 200 || status >= 300 || payload.empty())
        return;

    GInputStream* stream = g_memory_input_stream_new_from_data(payload.data(), payload.size(), nullptr);
    if (!stream)
        return;
    GError* error = nullptr;
    GdkPixbuf* pix = gdk_pixbuf_new_from_stream_at_scale(stream, 160, 160, TRUE, nullptr, &error);
    g_object_unref(stream);
    if (!pix) {
        if (error)
            g_error_free(error);
        return;
    }
    gtk_image_set_from_pixbuf(GTK_IMAGE(image_.gobj()), pix);
    g_object_unref(pix);
}

void MemoAuthImage::on_click_open()
{
    if (full_url_.empty())
        return;
    GError* error = nullptr;
    gtk_show_uri_on_window(nullptr, full_url_.c_str(), GDK_CURRENT_TIME, &error);
    if (error)
        g_error_free(error);
}

} // namespace cd
