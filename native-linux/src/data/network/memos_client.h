#pragma once

#include "domain/models.h"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

typedef struct _SoupSession SoupSession;

namespace cd {

class SecretStore;

class MemosClient {
public:
    MemosClient(SecretStore& secrets, SoupSession* session);

    [[nodiscard]] std::optional<std::string> base_url_opt() const;

    [[nodiscard]] std::pair<std::vector<MemosMemo>, std::optional<std::string>> list_memos(
        std::optional<std::string> page_token, std::optional<std::string> filter, MemoState state,
        std::string const& order_by = "display_time desc", int page_size = 50);

    [[nodiscard]] std::optional<MemosMemo> get_memo(std::string const& memo_id);

    [[nodiscard]] std::optional<MemosMemo> create_memo(std::string const& content, MemoVisibility visibility,
        std::vector<std::string> const& attachment_names = {});

    /** Builds `updateMask` from whichever of content/state/visibility is set. */
    [[nodiscard]] std::optional<MemosMemo> update_memo(std::string const& memo_id, std::optional<std::string> content,
        std::optional<MemoState> state, std::optional<MemoVisibility> visibility = std::nullopt);

    [[nodiscard]] bool delete_memo(std::string const& memo_id, bool force = false);

    [[nodiscard]] std::vector<MemosMemo> list_memo_comments(std::string const& memo_id,
        std::optional<std::string> page_token = std::nullopt);

    [[nodiscard]] std::optional<MemosMemo> create_memo_comment(
        std::string const& parent_memo_id, std::string const& content, MemoVisibility visibility);

    [[nodiscard]] std::vector<MemosAttachment> list_memo_attachments(std::string const& memo_id);

    [[nodiscard]] std::optional<MemosAttachment> create_attachment(std::string const& filename,
        std::string const& mime_type, std::vector<std::uint8_t> const& bytes,
        std::optional<std::string> memo_name = std::nullopt);

    [[nodiscard]] bool delete_attachments_batch(std::vector<std::string> const& names);

    [[nodiscard]] std::vector<MemoRelation> list_memo_relations(std::string const& memo_id);

    [[nodiscard]] std::optional<std::string> create_memo_share(std::string const& memo_id,
        std::optional<std::string> expire_time_iso = std::nullopt);

    [[nodiscard]] std::optional<MemosMemo> get_memo_by_share(std::string const& share_id);

private:
    SecretStore& secrets_;
    SoupSession* session_{};

    [[nodiscard]] std::map<std::string, std::string> bearer_headers() const;
    [[nodiscard]] std::optional<std::string> get_authed(std::string const& url) const;
    [[nodiscard]] std::optional<std::string> post_json(std::string const& url, std::string const& json_body) const;
    [[nodiscard]] std::optional<std::string> patch_json(std::string const& url, std::string const& json_body) const;
    [[nodiscard]] bool delete_url(std::string const& url) const;
    [[nodiscard]] std::optional<std::string> get_plain(std::string const& url) const;

    [[nodiscard]] static std::string escape_query(std::string const& s);
};

} // namespace cd
