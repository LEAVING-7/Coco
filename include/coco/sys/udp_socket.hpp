#pragma once
#include "socket.hpp"
namespace coco::sys {
class UdpSocket : Socket {
public:
  UdpSocket() noexcept = default;
  UdpSocket(UdpSocket&& socket) noexcept = default;
  auto operator=(UdpSocket&& socket) noexcept -> UdpSocket& = default;

  static auto bind(SocketAddr const& addr) -> std::pair<UdpSocket, std::errc>
  {
    // TODO: remove AF_INET
    auto [socket, errc] = Socket::create(AF_INET, SOCK_DGRAM);
    if (errc != std::errc{0}) {
      return {UdpSocket(), errc};
    }
    int opt = 1;
    socket.setopt(SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    errc = socket.bind(addr);
    if (errc != std::errc{0}) {
      return {UdpSocket(), errc};
    }
    return {UdpSocket(std::move(socket)), std::errc{0}};
  }
  static auto create(SocketAddr const& addr) noexcept -> std::pair<UdpSocket, std::errc>
  {
    // TODO: remove AF_INET
    auto [socket, errc] = Socket::create(AF_INET, SOCK_DGRAM);
    if (errc != std::errc{0}) {
      return {UdpSocket(), errc};
    } else {
      return {UdpSocket(std::move(socket)), std::errc{0}};
    }
  }
  auto recvfrom(std::span<std::byte> buf, SocketAddr& addr) noexcept -> decltype(auto)
  {
    return Socket::recvFrom(buf, addr);
  }
  auto sendto(std::span<std::byte const> buf, SocketAddr const& addr) noexcept -> decltype(auto)
  {
    return Socket::sendTo(buf, addr);
  }

private:
  UdpSocket(Socket&& socket) noexcept : Socket(std::move(socket)) {}
};
} // namespace coco::sys