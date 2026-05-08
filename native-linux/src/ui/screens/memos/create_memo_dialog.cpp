#include "create_memo_dialog.h"

#include "app_container.h"

#include <gio/gio.h>
#include <gtk/gtk.h>

#include <fstream>
#include <iterator>

namespace {

std::vector<std::uint8_t> read_file_bytes(std::string const& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return {};
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(f), {});
}

std::string basename_of(std::string const& path)
{
    auto pos = path.find_last_of('/');
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

} // namespace

namespace cd {

CreateMemoDialog::CreateMemoDialog(Gtk::Window& parent, AppContainer& app, std::string initial_text)
    : Gtk::Dialog("Create memo", parent, true)
    , app_(app)
{
    add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    add_button("_Create", Gtk::RESPONSE_OK);

    visibility_combo_.append("PRIVATE");
    visibility_combo_.append("PROTECTED");
    visibility_combo_.append("PUBLIC");
    visibility_combo_.set_active(0);

    content_view_.set_wrap_mode(Gtk::WRAP_WORD_CHAR);
    content_view_.get_buffer()->set_text(initial_text);

    auto* sc = Gtk::manage(new Gtk::ScrolledWindow());
    sc->set_min_content_height(220);
    sc->add(content_view_);

    attachments_label_.set_halign(Gtk::ALIGN_START);
    attachments_label_.set_text("No attachments");
    add_attachment_btn_.signal_clicked().connect(sigc::mem_fun(*this, &CreateMemoDialog::on_add_attachment));

    Gtk::Box& box = *get_content_area();
    box.set_spacing(8);
    box.pack_start(visibility_combo_, false, false);
    box.pack_start(*sc, true, true);
    box.pack_start(add_attachment_btn_, false, false);
    box.pack_start(attachments_label_, false, false);

    set_default_size(560, 420);
    show_all_children();
}

CreateMemoDialog::CreateMemoDialog(Gtk::Window& parent, AppContainer& app, MemosMemo const& existing)
    : Gtk::Dialog("Edit memo", parent, true)
    , app_(app)
    , edit_mode_(true)
    , editing_name_(existing.name)
{
    add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    add_button("_Save", Gtk::RESPONSE_OK);

    visibility_combo_.append("PRIVATE");
    visibility_combo_.append("PROTECTED");
    visibility_combo_.append("PUBLIC");
    switch (existing.visibility) {
    case MemoVisibility::Public: visibility_combo_.set_active(2); break;
    case MemoVisibility::Protected: visibility_combo_.set_active(1); break;
    default: visibility_combo_.set_active(0); break;
    }

    content_view_.set_wrap_mode(Gtk::WRAP_WORD_CHAR);
    content_view_.get_buffer()->set_text(existing.content);

    auto* sc = Gtk::manage(new Gtk::ScrolledWindow());
    sc->set_min_content_height(220);
    sc->add(content_view_);

    attachments_label_.set_halign(Gtk::ALIGN_START);
    attachments_label_.set_text("");

    Gtk::Box& box = *get_content_area();
    box.set_spacing(8);
    box.pack_start(visibility_combo_, false, false);
    box.pack_start(*sc, true, true);
    add_attachment_btn_.hide();
    attachments_label_.hide();

    set_default_size(560, 420);
    show_all_children();
}

std::string CreateMemoDialog::content()
{
    Gtk::TextBuffer::iterator b, e;
    content_view_.get_buffer()->get_bounds(b, e);
    return content_view_.get_buffer()->get_text(b, e);
}

MemoVisibility CreateMemoDialog::visibility() const
{
    std::string raw = visibility_combo_.get_active_text();
    if (raw == "PUBLIC")
        return MemoVisibility::Public;
    if (raw == "PROTECTED")
        return MemoVisibility::Protected;
    return MemoVisibility::Private;
}

void CreateMemoDialog::on_add_attachment()
{
    GtkWindow* parent = GTK_WINDOW(get_toplevel()->gobj());
    GtkFileChooserNative* native = gtk_file_chooser_native_new(
        "Attach file", parent, GTK_FILE_CHOOSER_ACTION_OPEN, "_Add", "_Cancel");
    int result = gtk_native_dialog_run(GTK_NATIVE_DIALOG(native));
    if (result != GTK_RESPONSE_ACCEPT) {
        g_object_unref(native);
        return;
    }

    GFile* file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(native));
    char* path = file ? g_file_get_path(file) : nullptr;
    if (file)
        g_object_unref(file);
    g_object_unref(native);
    if (!path)
        return;

    std::string full_path(path);
    g_free(path);

    auto bytes = read_file_bytes(full_path);
    if (bytes.empty())
        return;

    gboolean uncertain = FALSE;
    gchar* guess = g_content_type_guess(full_path.c_str(), bytes.data(), bytes.size(), &uncertain);
    std::string mime = guess ? guess : "application/octet-stream";
    if (guess)
        g_free(guess);

    attachments_.push_back(PendingAttachment{basename_of(full_path), mime, std::move(bytes)});
    attachments_label_.set_text(std::to_string(attachments_.size()) + " attachment(s) selected");
}

} // namespace cd
