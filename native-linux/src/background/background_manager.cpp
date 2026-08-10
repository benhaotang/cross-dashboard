#include "background_manager.h"
#include "background/background_command.h"
#include "background/background_content_builder.h"
#include "background/background_definition.h"
#include "background/background_renderer.h"
#include "app_container.h"
#include "data/prefs/prefs.h"
#include <filesystem>
#include <algorithm>

namespace cd {
BackgroundManager::BackgroundManager(AppContainer& app):app_(app){}
BackgroundManager::~BackgroundManager(){stop();}
void BackgroundManager::stop(){if(child_){g_subprocess_force_exit(child_);g_object_unref(child_);child_=nullptr;}}
bool BackgroundManager::refresh(std::string& message){
    auto command=app_.prefs().background_command();auto raw=app_.prefs().background_template_json();if(!command||command->empty()||!raw||raw->empty()){stop();message="Background updates are disabled";return false;}
    BackgroundTemplate definition;if(!parse_background_template(*raw,definition)||!definition.enabled){stop();message="No enabled background snapshot";return false;}
    std::filesystem::path dir=g_get_user_cache_dir();dir/="crossdashboard";auto file=(dir/"background.png").string();std::string error;
    BackgroundAppearance appearance;
    appearance.image_path=app_.prefs().background_image_path().value_or("");
    appearance.image_fit=app_.prefs().background_image_fit().value_or("fill");
    try { appearance.glass_opacity=std::clamp(std::stod(app_.prefs().background_glass_opacity().value_or("0.8")),0.5,1.0); } catch (...) {}
    appearance.dark=app_.prefs().theme().value_or("dark")!="light";
    if(!render_background_png(build_background_content(app_,definition),appearance,file,error)){message="Could not render background: "+error;return false;}
    std::vector<std::string> args;if(!expand_background_command(*command,file,args,error)){message=error;return false;}stop();
    std::vector<char const*> ptrs;for(auto const& a:args)ptrs.push_back(a.c_str());ptrs.push_back(nullptr);GError* launch_error{};
    auto const flags=static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDOUT_SILENCE|G_SUBPROCESS_FLAGS_STDERR_PIPE);
    child_=g_subprocess_newv(ptrs.data(),flags,&launch_error);if(!child_){message=launch_error?launch_error->message:"Could not launch background command";if(launch_error)g_error_free(launch_error);return false;}
    message="Background updated";return true;
}
}
