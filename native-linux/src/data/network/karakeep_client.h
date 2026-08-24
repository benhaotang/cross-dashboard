#pragma once

#include <optional>
#include <string>
#include <vector>

typedef struct _SoupSession SoupSession;

namespace cd {

class SecretStore;

struct KarakeepFolder {
    std::string id;
    std::string name;
    std::optional<std::string> parent_id;
};

class KarakeepClient final {
public:
    KarakeepClient(SecretStore& secrets, SoupSession* session);

    [[nodiscard]] std::vector<KarakeepFolder> list_folders() const;
    void save_urls(std::vector<std::string> const& urls, std::optional<std::string> const& folder_id) const;

private:
    [[nodiscard]] std::string api_base() const;
    [[nodiscard]] std::string request(char const* method, std::string const& path,
        std::optional<std::string> const& body = std::nullopt, bool allow_empty = false) const;

    SecretStore& secrets_;
    SoupSession* session_{};
};

} // namespace cd
