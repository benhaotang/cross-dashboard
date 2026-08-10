#pragma once

namespace cd::service_dbus {

inline constexpr char kBusName[] = "com.crossdashboard.Service";
inline constexpr char kObjectPath[] = "/com/crossdashboard/Service";
inline constexpr char kInterface[] = "com.crossdashboard.Service1";
inline constexpr char kSyncMethod[] = "Sync";
inline constexpr char kSyncCompletedSignal[] = "SyncCompleted";
inline constexpr char kRefreshBackgroundMethod[] = "RefreshBackground";
inline constexpr char kBackgroundUpdatedSignal[] = "BackgroundUpdated";
inline constexpr char kStartPomodoroMethod[] = "StartPomodoro";
inline constexpr char kGetPomodoroStateMethod[] = "GetPomodoroState";
inline constexpr char kPausePomodoroMethod[] = "PausePomodoro";
inline constexpr char kResumePomodoroMethod[] = "ResumePomodoro";
inline constexpr char kStopPomodoroMethod[] = "StopPomodoro";
inline constexpr char kSkipPomodoroMethod[] = "SkipPomodoro";
inline constexpr char kPomodoroStateChangedSignal[] = "PomodoroStateChanged";
// active, running, seconds left, phase duration, completed focus sessions,
// phase, target kind, target id, target title
inline constexpr char kPomodoroStateTupleType[] = "(bbiiissss)";

} // namespace cd::service_dbus
