#include "background_manager.h"
#include "background/background_command.h"
#include "background/background_content_builder.h"
#include "background/background_definition.h"
#include "background/background_renderer.h"
#include "app_container.h"
#include "data/prefs/prefs.h"
#include <filesystem>
#include <algorithm>
#include <optional>

namespace cd {
namespace {
std::optional<bool> portal_dark_preference()
{
    GError* error{}; auto* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (!bus) { if (error) g_error_free(error); return std::nullopt; }
    auto read=[&](char const* method){return g_dbus_connection_call_sync(bus,
        "org.freedesktop.portal.Desktop","/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Settings",method,
        g_variant_new("(ss)","org.freedesktop.appearance","color-scheme"),
        G_VARIANT_TYPE("(v)"),G_DBUS_CALL_FLAGS_NONE,1500,nullptr,&error);};
    auto* result=read("ReadOne");
    if(!result){if(error){g_error_free(error);error=nullptr;}result=read("Read");}
    g_object_unref(bus);
    if (!result) { if (error) g_error_free(error); return std::nullopt; }
    GVariant* value{}; g_variant_get(result, "(@v)", &value); g_variant_unref(result);
    while (g_variant_is_of_type(value, G_VARIANT_TYPE_VARIANT)) {
        auto* inner = g_variant_get_variant(value); g_variant_unref(value); value = inner;
    }
    std::optional<bool> dark;
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32)) {
        auto preference = g_variant_get_uint32(value);
        if (preference == 1) dark = true; else if (preference == 2) dark = false;
    }
    g_variant_unref(value); return dark;
}

std::string nonempty_or(std::optional<std::string> const& preferred, std::optional<std::string> const& fallback)
{
    return preferred && !preferred->empty() ? *preferred : fallback.value_or("");
}
}

BackgroundManager::BackgroundManager(AppContainer& app):app_(app){}
BackgroundManager::~BackgroundManager(){stop();}
void BackgroundManager::stop(){if(child_){g_subprocess_force_exit(child_);g_object_unref(child_);child_=nullptr;}}
bool BackgroundManager::refresh(std::string& message){
    auto command=app_.prefs().background_command();auto raw=app_.prefs().background_template_json();if(!command||command->empty()||!raw||raw->empty()){stop();message="Background updates are disabled";return false;}
    BackgroundTemplate definition;if(!parse_background_template(*raw,definition)||!definition.enabled){stop();message="No enabled background snapshot";return false;}
    std::filesystem::path dir=g_get_user_cache_dir();dir/="crossdashboard";auto file=(dir/"background.png").string();std::string error;
    BackgroundAppearance appearance;
    auto const both=app_.prefs().background_image_path();
    appearance.image_fit=app_.prefs().background_image_fit().value_or("fill");
    try { appearance.glass_opacity=std::clamp(std::stod(app_.prefs().background_glass_opacity().value_or("0.8")),0.5,1.0); } catch (...) {}
    auto const theme=app_.prefs().theme().value_or("auto");
    appearance.dark=theme=="dark"?true:theme=="light"?false:portal_dark_preference().value_or(false);
    appearance.image_path=appearance.dark
        ? nonempty_or(app_.prefs().background_dark_image_path(),both)
        : nonempty_or(app_.prefs().background_light_image_path(),both);
    if(!render_background_png(build_background_content(app_,definition),appearance,file,error)){message="Could not render background: "+error;return false;}
    std::vector<std::string> args;if(!expand_background_command(*command,file,args,error)){message=error;return false;}stop();
    std::vector<char const*> ptrs;for(auto const& a:args)ptrs.push_back(a.c_str());ptrs.push_back(nullptr);GError* launch_error{};
    auto const flags=static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDOUT_SILENCE|G_SUBPROCESS_FLAGS_STDERR_PIPE);
    child_=g_subprocess_newv(ptrs.data(),flags,&launch_error);if(!child_){message=launch_error?launch_error->message:"Could not launch background command";if(launch_error)g_error_free(launch_error);return false;}
    message="Background updated";return true;
}
}
