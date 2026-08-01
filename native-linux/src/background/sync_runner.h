#pragma once

#include <string>
#include <vector>

namespace cd {

class AppContainer;

/** Runs every configured backend sync, isolating failures so one backend cannot block the rest. */
[[nodiscard]] std::vector<std::string> sync_all(AppContainer& app);

} // namespace cd
