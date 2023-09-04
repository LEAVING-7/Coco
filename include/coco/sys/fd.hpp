#pragma once

namespace coco::sys {
inline auto lastErrc() -> std::errc { return std::errc(errno); }

class Fd {
public:
  Fd() noexcept : mFd(-1) {}
  Fd(int fd) noexcept : mFd(fd) {}
  Fd(Fd&& other) noexcept : mFd(std::exchange(other.mFd, -1)) {}
  auto operator=(Fd&& other) noexcept -> Fd&
  {
    mFd = std::exchange(other.mFd, -1);
    return *this;
  }

  auto close() noexcept -> std::errc
  {
    auto err = std::errc{0};
    if (mFd >= 0) {
      if (::close(mFd) == -1) {
        err = lastErrc();
      }
      mFd = -1;
    }
    return err;
  }
  auto fd() const noexcept -> int { return mFd; }
  auto valid() const noexcept -> bool { return mFd >= 0; }

protected:
  int mFd;
};
} // namespace coco::sys