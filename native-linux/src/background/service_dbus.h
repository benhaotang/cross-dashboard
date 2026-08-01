#pragma once

namespace cd::service_dbus {

inline constexpr char kBusName[] = "com.crossdashboard.Service";
inline constexpr char kObjectPath[] = "/com/crossdashboard/Service";
inline constexpr char kInterface[] = "com.crossdashboard.Service1";
inline constexpr char kSyncMethod[] = "Sync";
inline constexpr char kSyncCompletedSignal[] = "SyncCompleted";

} // namespace cd::service_dbus
