#include "background/notification_scheduler.h"

#include "app_container.h"
#include "data/db/event_dao.h"
#include "data/prefs/prefs.h"

#include <glib.h>
#include <libnotify/notify.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace cd {

namespace {

constexpr std::int64_t kMsPerSecond = 1000;
constexpr std::int64_t kMsPerDay = 24LL * 60LL * 60LL * kMsPerSecond;
constexpr std::int64_t kWindowMs = 7LL * kMsPerDay;
constexpr int kAlarmIdMod = 100000;

std::int64_t now_millis()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

int stable_alarm_id(std::string const& uid)
{
    auto const raw = static_cast<int>(std::hash<std::string>{}(uid));
    return std::abs(raw % kAlarmIdMod);
}

struct AlarmCtx {
    std::string title;
    std::string body;
};

gboolean fire_alarm(gpointer user_data)
{
    std::unique_ptr<AlarmCtx> ctx(static_cast<AlarmCtx*>(user_data));
    if (!ctx) return G_SOURCE_REMOVE;

    if (!notify_is_initted()) {
        (void)notify_init("cross-dashboard");
    }
    NotifyNotification* n = notify_notification_new(ctx->title.c_str(), ctx->body.c_str(), nullptr);
    GError* err = nullptr;
    (void)notify_notification_show(n, &err);
    if (err) g_error_free(err);
    g_object_unref(n);
    return G_SOURCE_REMOVE;
}

} // namespace

NotificationScheduler::NotificationScheduler(AppContainer& app)
    : app_(app)
{
}

NotificationScheduler::~NotificationScheduler()
{
    for (auto const& [_, timer] : timers_) {
        if (timer.source_id != 0) g_source_remove(timer.source_id);
    }
    timers_.clear();
}

void NotificationScheduler::reschedule_all()
{
    for (auto const& [_, timer] : timers_) {
        if (timer.source_id != 0) g_source_remove(timer.source_id);
    }
    timers_.clear();

    AppSettings const settings = merged_app_preferences(app_.prefs());
    if (!settings.notifications_enabled) return;

    EventDao event_dao(app_.db());
    auto events = event_dao.get_all();
    if (events.empty()) return;

    std::int64_t const now = now_millis();
    std::int64_t const upper = now + kWindowMs;
    std::int64_t const notify_offset = static_cast<std::int64_t>(settings.notification_minutes_before) * 60LL * 1000LL;

    for (auto const& ev : events) {
        std::int64_t const alarm_at = ev.start - notify_offset;
        if (alarm_at <= now || alarm_at > upper) continue;

        std::int64_t delay_ms = alarm_at - now;
        if (delay_ms < 1000) delay_ms = 1000;
        if (delay_ms > static_cast<std::int64_t>(G_MAXUINT)) continue;

        auto* ctx = new AlarmCtx{
            ev.summary.empty() ? std::string{"Event reminder"} : ev.summary,
            "Starts soon"};
        std::uint32_t source = g_timeout_add(
            static_cast<guint>(delay_ms), &fire_alarm, ctx);

        int const alarm_id = stable_alarm_id(ev.uid);
        timers_[alarm_id] = AlarmTimer{source, ev.uid};
    }
}

} // namespace cd
