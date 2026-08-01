#include "background/notification_scheduler.h"

#include "app_container.h"
#include "data/db/event_dao.h"
#include "data/prefs/prefs.h"

#include <glib.h>
#include <libnotify/notify.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <string>
#include <unordered_set>
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

void remove_source_if_present(std::uint32_t source_id)
{
    if (source_id != 0 && g_main_context_find_source_by_id(nullptr, source_id))
        g_source_remove(source_id);
}

int stable_alarm_id(std::string const& uid)
{
    auto const raw = static_cast<int>(std::hash<std::string>{}(uid));
    return std::abs(raw % kAlarmIdMod);
}

struct AlarmCtx {
    std::string title;
    std::string body;
    std::unordered_map<int, std::int64_t>* notified_alarm_times{};
    int alarm_id{};
    std::int64_t alarm_at{};
};

gboolean fire_alarm(gpointer user_data)
{
    auto* ctx = static_cast<AlarmCtx*>(user_data);
    if (!ctx) return G_SOURCE_REMOVE;
    if (ctx->notified_alarm_times) (*ctx->notified_alarm_times)[ctx->alarm_id] = ctx->alarm_at;

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

void destroy_alarm(gpointer user_data)
{
    delete static_cast<AlarmCtx*>(user_data);
}

} // namespace

NotificationScheduler::NotificationScheduler(AppContainer& app)
    : app_(app)
{
}

NotificationScheduler::~NotificationScheduler()
{
    for (auto const& [_, timer] : timers_) {
        remove_source_if_present(timer.source_id);
    }
    timers_.clear();
}

void NotificationScheduler::reschedule_all()
{
    for (auto const& [_, timer] : timers_) {
        remove_source_if_present(timer.source_id);
    }
    timers_.clear();

    AppSettings const settings = merged_app_preferences(app_.prefs());
    if (!settings.notifications_enabled) {
        notified_alarm_times_.clear();
        return;
    }

    EventDao event_dao(app_.db());
    auto events = event_dao.get_all();
    if (events.empty()) return;

    std::int64_t const now = now_millis();
    std::int64_t const upper = now + kWindowMs;
    std::int64_t const notify_offset = static_cast<std::int64_t>(settings.notification_minutes_before) * 60LL * 1000LL;
    std::unordered_set<int> live_ids;

    for (auto const& ev : events) {
        std::int64_t const alarm_at = ev.start - notify_offset;
        // If the service starts inside the reminder window, notify immediately rather than
        // losing the reminder merely because its ideal fire time passed while the service was down.
        if (ev.start <= now || alarm_at > upper) continue;

        int const alarm_id = stable_alarm_id(ev.uid);
        live_ids.insert(alarm_id);
        auto const notified = notified_alarm_times_.find(alarm_id);
        if (notified != notified_alarm_times_.end() && notified->second == alarm_at) continue;

        std::int64_t delay_ms = alarm_at - now;
        if (delay_ms < 1000) delay_ms = 1000;
        if (delay_ms > static_cast<std::int64_t>(G_MAXUINT)) continue;

        auto* ctx = new AlarmCtx{
            ev.summary.empty() ? std::string{"Event reminder"} : ev.summary,
            "Starts soon", &notified_alarm_times_, alarm_id, alarm_at};
        std::uint32_t source = g_timeout_add_full(G_PRIORITY_DEFAULT,
            static_cast<guint>(delay_ms), &fire_alarm, ctx, &destroy_alarm);

        timers_[alarm_id] = AlarmTimer{source, ev.uid};
    }

    std::erase_if(notified_alarm_times_, [&](auto const& entry) { return !live_ids.contains(entry.first); });
}

} // namespace cd
