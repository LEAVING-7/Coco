#pragma once
#include "coco/__preclude.hpp"

#include "socket.hpp"
#include "stream.hpp"

namespace coco::net {
class TcpListener : private Socket {
public:
  TcpListener() noexcept = default;
  TcpListener(TcpListener&& listener) noexcept = default;

  static auto bind(SocketAddr const& addr) -> std::pair<TcpListener, std::errc>
  {
    auto [socket, errc] = Socket::create(addr);
    if (errc != std::errc{0}) {
      return {TcpListener(), errc};
    }
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
  auto recv(std::span<std::byte> buf) noexcept -> decltype(auto) { return Socket::recv(buf); }
  auto send(std::span<std::byte const> buf) noexcept -> decltype(auto) { return Socket::send(buf); }

private:
  TcpListener(Socket&& socket) noexcept : Socket(std::move(socket)) {}
};
} // namespace coco::net