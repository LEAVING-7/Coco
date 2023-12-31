#pragma once

#include "coco/sys/socket.hpp"
#include "coco/task.hpp"

namespace coco::sys {
class TcpStream : private Socket {
public:
  TcpStream() noexcept = default;
  TcpStream(TcpStream&& stream) noexcept = default;
  auto operator=(TcpStream&& stream) noexcept -> TcpStream& = default;

  static auto connect(SocketAddr const& addr) -> coco::Task<std::pair<TcpStream, std::errc>>
  {
    auto [socket, errc] = Socket::create(addr, Socket::Stream);
    if (errc != std::errc{0}) {
      co_return {TcpStream(), errc};
    }
    errc = co_await socket.connect(addr);
    if (errc != std::errc{0}) {
      co_return {TcpStream(), errc};
    }
    co_return {TcpStream(std::move(socket)), std::errc{0}};
  }
  template <typename Rep, typename Period>
  static auto connect(SocketAddr const& addr, std::chrono::duration<Rep, Period> timeout)
      -> coco::Task<std::pair<TcpStream, std::errc>>
  {
    auto [socket, errc] = Socket::create(addr, Socket::Stream);
    if (errc != std::errc{0}) {
      co_return {TcpStream(), errc};
    }
    errc = co_await socket.connect(addr, timeout);
    if (errc != std::errc{0}) {
      co_return {TcpStream(), errc};
    }
    co_return {TcpStream(std::move(socket)), std::errc{0}};
  }
  static auto from(Socket&& socket) noexcept -> TcpStream { return TcpStream(std::move(socket)); }
  auto recv(std::span<std::byte> buf) noexcept -> decltype(auto) { return Socket::recv(buf); }
  auto send(std::span<std::byte const> buf) noexcept -> decltype(auto) { return Socket::send(buf); }
  auto send(std::span<std::byte const> buf, std::chrono::milliseconds timeout) noexcept -> decltype(auto)
  {
    return Socket::send(buf, timeout);
  }
  auto recv(std::span<std::byte> buf, std::chrono::milliseconds timeout) noexcept -> decltype(auto)
  {
    return Socket::recv(buf, timeout);
  }
  auto close() noexcept -> decltype(auto) { return Socket::close(); }

private:
  TcpStream(Socket&& socket) noexcept : Socket(std::move(socket)) {}
};
} // namespace coco::sys