#include "data/repository/operation_lock.h"

#include <glib.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace cd {

OperationLock::OperationLock()
{
    char const* runtime = g_get_user_runtime_dir();
    std::filesystem::path directory = runtime && runtime[0] != '\0'
        ? std::filesystem::path{runtime}
        : std::filesystem::path{g_get_user_cache_dir()} / "crossdashboard";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error)
        throw std::runtime_error("cannot create Cross-Dashboard lock directory: " + error.message());

    std::string const path = (directory / "crossdashboard-operation.lock").string();
    fd_ = ::open(path.c_str(), O_CREAT | O_CLOEXEC | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd_ < 0)
        throw std::runtime_error("cannot open Cross-Dashboard operation lock: "
            + std::string{std::strerror(errno)});

    while (::flock(fd_, LOCK_EX) != 0) {
        if (errno == EINTR) continue;
        int const saved_errno = errno;
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error("cannot acquire Cross-Dashboard operation lock: "
            + std::string{std::strerror(saved_errno)});
    }
}

OperationLock::~OperationLock()
{
    if (fd_ < 0) return;
    (void)::flock(fd_, LOCK_UN);
    ::close(fd_);
}

} // namespace cd
