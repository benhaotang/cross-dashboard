#include "app_container.h"

#include <stdexcept>

namespace cd {

AppContainer::AppContainer()
    : db_{}
    , secrets_{}
    , prefs_{}
    , soup_{}
    , event_dao_{db_}
    , task_dao_{db_}
    , note_dao_{db_}
    , issue_dao_{db_}
    , memo_dao_{db_}
    , stats_dao_{db_}
{
    (void)apply_timezone_override(prefs_.timezone_override());
    SoupSession* ss = soup_session_new();
    if (!ss) throw std::runtime_error("SoupSession init failed");
    soup_.reset(ss);

    caldav_.emplace(secrets_, soup_.get());
    gitea_.emplace(secrets_, soup_.get());
    memos_client_.emplace(secrets_, soup_.get());
    nextcloud_.emplace(soup_.get());

    event_repo_.emplace(event_dao_, *caldav_, secrets_);
    task_repo_.emplace(task_dao_, stats_dao_, *caldav_);
    note_repo_.emplace(note_dao_, *caldav_);
    issue_repo_.emplace(issue_dao_, stats_dao_, *gitea_);
    memo_repository_.emplace(memo_dao_, *memos_client_);
    stats_repo_.emplace(stats_dao_);
}

AppContainer::~AppContainer() = default;

} // namespace cd
