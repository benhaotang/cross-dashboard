#include "background/sync_runner.h"

#include "app_container.h"
#include "data/prefs/prefs.h"
#include "data/repository/repo_utils.h"

#include <exception>
#include <functional>
#include <string>

namespace cd {

std::vector<std::string> sync_all(AppContainer& app)
{
    auto const calendars =
        calendars_from_selected_json(app.secrets().get(CredentialKey::CALDAV_SELECTED_CALENDARS));
    auto const repos = repos_from_cred_string(app.secrets().get(CredentialKey::GITEA_REPOS));
    std::vector<std::string> errors;

    auto run = [&errors](char const* name, std::function<void()> const& operation) {
        try {
            operation();
        }
        catch (std::exception const& error) {
            errors.emplace_back(std::string{name} + ": " + error.what());
        }
        catch (...) {
            errors.emplace_back(std::string{name} + ": unknown error");
        }
    };

    run("events", [&] { app.events().sync_many(calendars); });
    run("tasks", [&] { app.tasks().sync_many(calendars); });
    run("notes", [&] { app.notes().sync_many(calendars); });
    run("issues", [&] { app.issues().sync_many(repos); });
    run("capture", [&] { app.memos_repository().sync_all(); });
    return errors;
}

} // namespace cd
