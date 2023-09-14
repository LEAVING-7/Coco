#pragma once

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include "coco/sys/fd.hpp"
#include "coco/sys/socket_addr.hpp"
#include "coco/sys/socket_awaiters.hpp"

namespace coco::sys {

class Socket : public Fd {
public:
  Socket() noexcept = default;
  Socket(int fd) noexcept : Fd(fd) {}
  Socket(Socket&& socket) noexcept = default;
  auto operator=(Socket&& socket) noexcept -> Socket& = default;
  ~Socket() noexcept { close(); }

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
    return {fd, std::errc{0}};
  }

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
  auto recv(std::span<std::byte> buf, Duration duration) noexcept -> decltype(auto)
  {
    return detail::RecvTimeoutAwaiter(mFd, buf, duration);
  }
  auto send(std::span<std::byte const> buf, int flags = 0) noexcept -> decltype(auto)
  {
    return detail::SendAwaiter(mFd, buf);
  }
  auto send(std::span<std::byte const> buf, Duration duration) noexcept -> decltype(auto)
  {
    return detail::SendTimeoutAwaiter(mFd, buf, duration);
  }
  auto sendTo(std::span<std::byte const> buf, SocketAddr const& addr, int flags = 0) noexcept -> decltype(auto)
  {
    return detail::SendToAwaiter(mFd, buf, addr);
  }
  auto recvFrom(std::span<std::byte> buf, SocketAddr& addr, int flags = 0) noexcept -> decltype(auto)
  {
    return detail::RecvFromAwaiter(mFd, buf, addr);
  }
  auto connect(SocketAddr addr, Duration duration) noexcept -> decltype(auto)
  {
    return detail::ConnectTimeoutAwaiter(mFd, addr, duration);
  }
  auto accept(Duration duration) noexcept -> decltype(auto) { return detail::AcceptTimeoutAwaiter(mFd, duration); }
  auto accept(int flags = 0) noexcept -> decltype(auto) { return detail::AcceptAwaiter(mFd); }
  auto connect(SocketAddr addr) noexcept -> decltype(auto) { return detail::ConnectAwaiter(mFd, addr); }

  auto setopt(int level, int optname, void const* optval, socklen_t optlen) noexcept -> std::errc
  {
    if (::setsockopt(mFd, level, optname, optval, optlen) == -1) {
      return lastErrc();
    }
    return std::errc{0};
  }

  auto getopt(int level, int optname, void* optval, socklen_t* optlen) noexcept -> std::errc
  {
    if (::getsockopt(mFd, level, optname, optval, optlen) == -1) {
      return lastErrc();
    }
    return std::errc{0};
  }
};
} // namespace coco::sys