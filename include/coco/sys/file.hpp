#pragma once

#include "coco/sys/fd.hpp"
#include "coco/sys/file_awaiters.hpp"

namespace coco::sys {
class File : public Fd {
public:
  File() noexcept = default;
  File(int fd) noexcept : Fd(fd) {}
  File(File&& file) noexcept = default;
  auto operator=(File&& file) noexcept -> File& = default;
  ~File() noexcept { close(); }

  static auto open(char const* path, int flags, mode_t mode = 0) noexcept -> std::pair<File, std::errc>
  {
    auto fd = ::open(path, flags, mode);
    if (fd < 0) {
      return {File(), lastErrc()};
    } else {
      return {File(fd), std::errc(0)};
    }
  }
  auto read(std::span<std::byte> buf, off_t offset) noexcept -> decltype(auto)
  {
    return detail::ReadAwaiter(mFd, buf, offset);
  }
  auto write(std::span<std::byte const> buf, off_t offset) noexcept -> decltype(auto)
  {
    return detail::WriteAwaiter(mFd, buf, offset);
  }
};
} // namespace coco::sys
