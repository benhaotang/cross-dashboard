#pragma once
#include <gio/gio.h>
#include <string>
namespace cd { class AppContainer; class BackgroundManager final { public: explicit BackgroundManager(AppContainer&); ~BackgroundManager(); bool refresh(std::string& message); void stop(); private: AppContainer& app_; GSubprocess* child_{}; }; }
