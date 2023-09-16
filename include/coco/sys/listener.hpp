#pragma once

#include "socket.hpp"
#include "stream.hpp"

namespace coco::sys {
class TcpListener : private Socket {
public:
  TcpListener() noexcept = default;
  TcpListener(TcpListener&& listener) noexcept = default;

  static auto bind(SocketAddr const& addr) -> std::pair<TcpListener, std::errc>
  {
    auto [socket, errc] = Socket::create(addr, Socket::Stream);
    if (errc != std::errc{0}) {
      return {TcpListener(), errc};
    }
    int opt = 1;
    socket.setopt(SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    errc = socket.bind(addr);
    if (errc != std::errc{0}) {
      return {TcpListener(), errc};
    }
    errc = socket.listen(128);
    if (errc != std::errc{0}) {
      return {TcpListener(), errc};
    }
    return {TcpListener(std::move(socket)), std::errc{0}};
  }

  auto accept() noexcept -> Task<std::pair<TcpStream, std::errc>>
  {
    auto [socket, errc] = co_await Socket::accept();
    if (errc != std::errc{0}) {
      co_return {TcpStream(), errc};
    }
    co_return {TcpStream::from(std::move(socket)), std::errc{0}};
  }
  template <typename Rep, typename Period>
  auto accept(std::chrono::duration<Rep, Period> timeout) noexcept -> Task<std::pair<TcpStream, std::errc>>
  {
    auto [socket, errc] = co_await Socket::accept(timeout);
    if (errc != std::errc{0}) {
      co_return {TcpStream(), errc};
    }
    co_return {TcpStream::from(std::move(socket)), std::errc{0}};
  }

  // auto acceptMulitshot(Task<> task) noexcept -> -
  // {
  // }

  auto recv(std::span<std::byte> buf) noexcept -> decltype(auto) { return Socket::recv(buf); }
  auto send(std::span<std::byte const> buf) noexcept -> decltype(auto) { return Socket::send(buf); }
  auto sendTimeout(std::span<std::byte const> buf, std::chrono::milliseconds timeout) noexcept -> decltype(auto)
  {
    return Socket::send(buf, timeout);
  }
  auto recvTimeout(std::span<std::byte> buf, std::chrono::milliseconds timeout) noexcept -> decltype(auto)
  {
    return Socket::recv(buf, timeout);
  }

private:
  bool mShot = false;
  TcpListener(Socket&& socket) noexcept : Socket(std::move(socket)) {}
};
} // namespace coco::sys