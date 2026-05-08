#pragma once

#include <gtkmm/application.h>
#include <giomm/applicationcommandline.h>

#include <memory>

namespace cd {

class AppContainer;
class AppViewModel;
class AppWindow;
class NotificationScheduler;
class PomodoroStatusItem;
class SyncScheduler;

class CdApplication final : public Gtk::Application {
public:
    static Glib::RefPtr<CdApplication> create();

protected:
    void on_activate() override;
    void on_open(const Gio::Application::type_vec_files& files, const Glib::ustring& hint) override;
    int on_command_line(Glib::RefPtr<Gio::ApplicationCommandLine> const& command_line) override;

private:
    CdApplication();

    void ensure_init();
    void present_main();

    std::unique_ptr<AppContainer> container_;
    std::unique_ptr<AppViewModel> model_;
    std::unique_ptr<NotificationScheduler> notifications_;
    std::unique_ptr<SyncScheduler> sync_scheduler_;
    std::unique_ptr<PomodoroStatusItem> pomodoro_status_item_;
    AppWindow* window_{nullptr};
    bool present_signal_bound_{false};
};

} // namespace cd
