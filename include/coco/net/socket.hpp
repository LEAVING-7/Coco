#pragma once
#include "coco/__preclude.hpp"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include "coco/net/socket_addr.hpp"
#include "coco/net/socket_awaiters.hpp"

namespace coco::net {
inline auto lastErrc() -> std::errc { return std::errc(errno); }

class Socket {
public:
  Socket() noexcept = default;
  Socket(int fd) noexcept : mFd(fd) {}
  Socket(Socket&& socket) noexcept : mFd(std::exchange(socket.mFd, -1)) {}

  static auto create(SocketAddr const& addr) noexcept -> std::pair<Socket, std::errc>
  {
    return create(addr.domain(), addr.type());
  }
  static auto create(int domain, int type, int protocol = 0) noexcept -> std::pair<Socket, std::errc>
  {
    auto fd = ::socket(domain, type, protocol);
    if (fd == -1) {
      return {-1, lastErrc()};
    }
    return {Socket(fd), std::errc{0}};
  }
  auto operator=(Socket&& socket) noexcept -> Socket&
  {
    mFd = std::exchange(socket.mFd, -1);
    return *this;
  }
  ~Socket() noexcept { close(); }

  auto bind(SocketAddr const& addr) noexcept -> std::errc
  {
    auto v4 = addr.toV4();
    return bind(reinterpret_cast<sockaddr const*>(&v4), sizeof(v4));
  }
  auto bind(sockaddr const* addr, socklen_t len) noexcept -> std::errc
  {
    if (::bind(mFd, addr, len) == -1) {
      return lastErrc();
    }
    return std::errc{0};
  }

  auto listen(int backlog) noexcept -> std::errc
  {
    if (::listen(mFd, backlog) == -1) {
      return lastErrc();
    }
    return std::errc{0};
  }

  auto recv(std::span<std::byte> buf, int flags = 0) noexcept -> decltype(auto)
  {
    return detail::RecvAwaiter(mFd, buf);
  }
  auto send(std::span<std::byte const> buf, int flags = 0) noexcept -> decltype(auto)
  {
    return detail::SendAwaiter(mFd, buf);
  }
  auto accept(int flags = 0) noexcept -> decltype(auto) { return detail::AcceptAwaiter(mFd); }
  auto connect(SocketAddr addr) noexcept -> decltype(auto) { return detail::ConnectAwaiter(mFd, addr); }

  auto close() noexcept -> std::errc
  {
    auto err = std::errc{0};
    if (mFd != -1) {
      if (::close(mFd) == -1) {
        err = lastErrc();
      }
      mFd = -1;
    }
    return err;
  }

  auto fd() const noexcept -> int { return mFd; }

private:
  int mFd{-1};
};
} // namespace coco::net