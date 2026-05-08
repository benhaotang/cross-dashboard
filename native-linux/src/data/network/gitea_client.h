#pragma once

#include "domain/models.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

typedef struct _SoupSession SoupSession;

namespace cd {

class SecretStore;

class GiteaClient {
public:
    GiteaClient(SecretStore& secrets, SoupSession* session);

    [[nodiscard]] std::vector<GiteaIssue> fetch_issues(std::vector<std::string> const& repositories,
        std::string const& state);

    GiteaIssue update_issue(
        std::string const& repo, int number, std::optional<std::string> title, std::optional<std::string> body,
        std::optional<std::string> state);

    [[nodiscard]] std::vector<GiteaComment> fetch_comments(std::string const& repo, int number);

    GiteaComment add_comment(std::string const& repo, int number, std::string const& body);

    [[nodiscard]] std::vector<GiteaLabel> fetch_labels(std::string const& repo);
    GiteaLabel create_repo_label(std::string const& repo, std::string const& name, std::string const& color);
    void replace_issue_labels(std::string const& repo, int number, std::vector<std::int64_t> label_ids);

    [[nodiscard]] std::vector<GiteaAttachment> fetch_issue_attachments(std::string const& repo, int issue_number);
    [[nodiscard]] std::vector<GiteaAttachment> fetch_comment_attachments(
        std::string const& repo, std::int64_t comment_id);

    /** Returns download URL. */
    std::string upload_issue_attachment(std::string const& repo, int issue_number,
        std::string const& file_name, std::vector<std::uint8_t> const& bytes, std::string const& mime_type);

    std::string upload_comment_attachment(std::string const& repo, std::int64_t comment_id,
        std::string const& file_name, std::vector<std::uint8_t> const& bytes, std::string const& mime_type);

    GiteaIssue create_issue(std::string const& repo, std::string const& title, std::string const& body);

private:
    SecretStore& secrets_;
    SoupSession* session_{};

    [[nodiscard]] std::optional<std::string> instance_url() const;
    [[nodiscard]] std::map<std::string, std::string> auth_headers() const;

    [[nodiscard]] std::optional<std::string> get(std::string const& url) const;
    [[nodiscard]] std::optional<std::string> post(std::string const& url, std::string const& json_body) const;
    [[nodiscard]] std::optional<std::string> patch(std::string const& url, std::string const& json_body) const;
    [[nodiscard]] std::optional<std::string> put(std::string const& url, std::string const& json_body) const;
};

} // namespace cd
