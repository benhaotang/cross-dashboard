#include "background/sync_scheduler.h"

#include "app_container.h"
#include "background/notification_scheduler.h"
#include "data/prefs/prefs.h"
#include "data/repository/repo_utils.h"

#include <glib.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace cd {

namespace {

gboolean tick_sync(gpointer user_data)
{
    auto* self = static_cast<SyncScheduler*>(user_data);
    if (!self) return G_SOURCE_REMOVE;
    self->sync_once();
    return G_SOURCE_CONTINUE;
}

bool write_if_missing(std::filesystem::path const& path, std::string const& contents)
{
    if (std::filesystem::exists(path)) return true;
    std::ofstream out(path);
    if (!out.good()) return false;
    out << contents;
    return out.good();
}

} // namespace

SyncScheduler::SyncScheduler(AppContainer& app, NotificationScheduler& notifications)
    : app_(app)
    , notifications_(notifications)
{
}

SyncScheduler::~SyncScheduler()
{
    stop();
}

void SyncScheduler::start(int interval_seconds)
{
    stop();

    if (interval_seconds < 30) interval_seconds = 30;
    (void)ensure_systemd_units(interval_seconds);

    source_id_ = g_timeout_add_seconds(
        static_cast<guint>(interval_seconds), &tick_sync, this);
}

void SyncScheduler::stop()
{
    if (source_id_ != 0) {
        g_source_remove(source_id_);
        source_id_ = 0;
    }
}

void SyncScheduler::sync_once()
{
    auto calendars = calendars_from_selected_json(app_.secrets().get(CredentialKey::CALDAV_SELECTED_CALENDARS));
    auto repos = repos_from_cred_string(app_.secrets().get(CredentialKey::GITEA_REPOS));

    try {
        app_.events().sync_many(calendars);
        app_.tasks().sync_many(calendars);
        app_.notes().sync_many(calendars);
        app_.issues().sync_many(repos);
        app_.memos_repository().sync_all();
    }
    catch (...) {
        // Keep scheduler alive on transient network or parsing failures.
    }

    notifications_.reschedule_all();
}

bool SyncScheduler::ensure_systemd_units(int interval_seconds)
{
    auto const config_home = std::filesystem::path(g_get_user_config_dir());
    auto const unit_dir = config_home / "systemd" / "user";
    std::error_code ec;
    std::filesystem::create_directories(unit_dir, ec);
    if (ec) return false;

    int const minutes = interval_seconds / 60;
    int const safe_minutes = minutes <= 0 ? 1 : minutes;

    std::string const service = R"([Unit]
Description=Cross-Dashboard background sync service
After=graphical-session.target

[Service]
Type=oneshot
ExecStart=cross-dashboard --reschedule-alarms
)";

    std::string const timer = "[Unit]\n"
                              "Description=Cross-Dashboard periodic sync timer\n\n"
                              "[Timer]\n"
                              "OnBootSec=2min\n"
                              "OnUnitActiveSec="
        + std::to_string(safe_minutes)
        + "min\n"
          "Unit=crossdashboard-sync.service\n"
          "Persistent=true\n\n"
          "[Install]\n"
          "WantedBy=timers.target\n";

    bool ok_service = write_if_missing(unit_dir / "crossdashboard-sync.service", service);
    bool ok_timer = write_if_missing(unit_dir / "crossdashboard-sync.timer", timer);
    return ok_service && ok_timer;
}

} // namespace cd
