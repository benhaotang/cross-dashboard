#include "application.h"

#include "app_container.h"
#include "app_viewmodel.h"
#include "app_window.h"
#include "background/pomodoro_status_item.h"
#include "background/service_dbus.h"
#include "background/sync_scheduler.h"
#include "data/prefs/prefs.h"

#include <glib.h>

#include <algorithm>
#include <string>
#include <vector>

namespace cd {

namespace {

bool request_service_sync_compat()
{
    GError* error = nullptr;
    GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (!bus) {
        if (error) g_error_free(error);
        return false;
    }
    GVariant* reply = g_dbus_connection_call_sync(bus, service_dbus::kBusName,
        service_dbus::kObjectPath, service_dbus::kInterface, service_dbus::kSyncMethod, nullptr,
        G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, 10000, nullptr, &error);
    bool const ok = reply != nullptr;
    if (reply) g_variant_unref(reply);
    if (error) g_error_free(error);
    g_object_unref(bus);
    return ok;
}

void route_open_uri(Glib::ustring const& uri_ustr, AppViewModel& vm)
{
    std::string const uri = uri_ustr.raw();

    if (uri.find("crossdashboard://tasks") != std::string::npos
        && (uri.find("action=add") != std::string::npos || uri.find("action%3Dadd") != std::string::npos)) {
        vm.trigger_new_task();
        return;
    }

    if (uri.find("crossdashboard://capture") == std::string::npos)
    {
        if (uri.rfind("file://", 0) == 0) {
            char* path = g_filename_from_uri(uri.c_str(), nullptr, nullptr);
            if (!path)
                return;
            gchar* content = nullptr;
            gsize len = 0;
            if (g_file_get_contents(path, &content, &len, nullptr) && content) {
                vm.trigger_capture(std::string(content, len));
                g_free(content);
            }
            g_free(path);
            return;
        }
        if (uri.rfind("http://", 0) == 0 || uri.rfind("https://", 0) == 0)
            vm.trigger_capture(uri);
        return;
    }

    std::size_t const q = uri.find('?');
    if (q == std::string::npos)
        return;

    std::string query = uri.substr(q + 1);
    std::size_t const t = query.find("text=");
    if (t == std::string::npos)
        return;

    std::string enc = query.substr(t + 5);
    std::size_t const amp = enc.find('&');
    if (amp != std::string::npos)
        enc = enc.substr(0, amp);

    char* dec = g_uri_unescape_string(enc.c_str(), nullptr);
    if (!dec)
        return;
    vm.trigger_capture(std::string{dec});
    g_free(dec);
}

} // namespace

Glib::RefPtr<CdApplication> CdApplication::create()
{
    return Glib::RefPtr<CdApplication>(new CdApplication());
}

CdApplication::CdApplication()
    : Gtk::Application("com.crossdashboard.app",
          Gio::APPLICATION_HANDLES_OPEN | Gio::APPLICATION_HANDLES_COMMAND_LINE)
{
}

void CdApplication::ensure_init()
{
    if (!container_)
        container_ = std::make_unique<AppContainer>();
    if (!model_)
        model_ = std::make_unique<AppViewModel>(*container_);
    if (!sync_scheduler_)
        sync_scheduler_ = std::make_unique<SyncScheduler>();
    if (!pomodoro_status_item_)
        pomodoro_status_item_ = std::make_unique<PomodoroStatusItem>(*model_);
    if (!present_signal_bound_) {
        model_->signal_present_window_requested.connect(sigc::mem_fun(*this, &CdApplication::present_main));
        present_signal_bound_ = true;
    }
}

void CdApplication::present_main()
{
    ensure_init();
    if (!window_) {
        window_ = Gtk::manage(new AppWindow(*container_, *model_, *sync_scheduler_));
        add_window(*window_);
    }
    window_->present();
}

void CdApplication::on_activate()
{
    ensure_init();
    AppSettings const settings = merged_app_preferences(container_->prefs());
    sync_scheduler_->start(settings.widget_sync_interval_minutes * 60);
    present_main();
}

void CdApplication::on_open(const Gio::Application::type_vec_files& files, const Glib::ustring& hint)
{
    (void)hint;
    ensure_init();
    for (auto const& f : files)
        route_open_uri(f->get_uri(), *model_);
    present_main();
}

int CdApplication::on_command_line(Glib::RefPtr<Gio::ApplicationCommandLine> const& command_line)
{
    ensure_init();
    int argc = 0;
    char** argv = command_line->get_arguments(argc);
    bool sync_only = false;
    for (int i = 0; i < argc; ++i) {
        if (argv[i] && (std::string(argv[i]) == "--reschedule-alarms"
                || std::string(argv[i]) == "--sync")) {
            sync_only = true;
            break;
        }
    }
    if (argv) g_strfreev(argv);

    if (sync_only) {
        return request_service_sync_compat() ? 0 : 1;
    }

    activate();
    return 0;
}

} // namespace cd
