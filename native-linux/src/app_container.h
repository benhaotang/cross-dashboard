#pragma once

#include "data/db/database.h"
#include "data/db/event_dao.h"
#include "data/db/issue_dao.h"
#include "data/db/memo_dao.h"
#include "data/db/note_dao.h"
#include "data/db/stats_dao.h"
#include "data/db/task_dao.h"
#include "data/network/caldav_client.h"
#include "data/network/gitea_client.h"
#include "data/network/karakeep_client.h"
#include "data/network/memos_client.h"
#include "data/network/nextcloud_login_flow.h"
#include "data/prefs/prefs.h"
#include "data/repository/repositories.h"

#include <libsoup/soup.h>

#include <memory>
#include <optional>

namespace cd {

struct SoupSessionDeleter {
    void operator()(SoupSession* p) const
    {
        if (p) g_object_unref(p);
    }
};

using SoupSessionPtr = std::unique_ptr<SoupSession, SoupSessionDeleter>;

/** Owns SQLite, credentials, preferences, Soup session, DAOs, network clients and repositories — Phase 1 backbone. */
class AppContainer final {
public:
    AppContainer();
    ~AppContainer();

    AppContainer(AppContainer const&) = delete;
    AppContainer& operator=(AppContainer const&) = delete;

    SoupSession* soup_session() const { return soup_.get(); }

    Database& db() { return db_; }
    SecretStore& secrets() { return secrets_; }
    AppPreferences& prefs() { return prefs_; }

    CalDavClient& caldav() { return *caldav_; }
    GiteaClient& gitea() { return *gitea_; }
    MemosClient& memos_client() { return *memos_client_; }
    KarakeepClient& karakeep() { return *karakeep_; }

    NextcloudLoginFlow& nextcloud_login_flow() { return *nextcloud_; }

    EventRepository& events() { return *event_repo_; }
    TaskRepository& tasks() { return *task_repo_; }
    NoteRepository& notes() { return *note_repo_; }
    IssueRepository& issues() { return *issue_repo_; }
    MemoRepository& memos_repository() { return *memo_repository_; }
    StatsRepository& stats() { return *stats_repo_; }

private:
    Database db_;
    SecretStore secrets_;
    AppPreferences prefs_;

    SoupSessionPtr soup_;
    std::optional<CalDavClient> caldav_;
    std::optional<GiteaClient> gitea_;
    std::optional<MemosClient> memos_client_;
    std::optional<KarakeepClient> karakeep_;
    std::optional<NextcloudLoginFlow> nextcloud_;

    EventDao event_dao_;
    TaskDao task_dao_;
    NoteDao note_dao_;
    IssueDao issue_dao_;
    MemoDao memo_dao_;
    DailyStatsDao stats_dao_;

    std::optional<EventRepository> event_repo_;
    std::optional<TaskRepository> task_repo_;
    std::optional<NoteRepository> note_repo_;
    std::optional<IssueRepository> issue_repo_;
    std::optional<MemoRepository> memo_repository_;
    std::optional<StatsRepository> stats_repo_;
};

} // namespace cd
