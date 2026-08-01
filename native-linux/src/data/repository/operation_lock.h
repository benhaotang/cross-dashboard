#pragma once

namespace cd {

/**
 * Cross-process lock for remote mutations and cache replacement.
 *
 * The GUI, CLI, and systemd user service all share the same runtime lock file, preventing a
 * backend fetch from replacing a cache while another process is creating/updating an item.
 */
class OperationLock final {
public:
    OperationLock();
    ~OperationLock();

    OperationLock(OperationLock const&) = delete;
    OperationLock& operator=(OperationLock const&) = delete;

private:
    int fd_{-1};
};

} // namespace cd
