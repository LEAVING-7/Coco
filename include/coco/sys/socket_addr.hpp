#pragma once

#include <netinet/in.h>
#include <sys/socket.h>

namespace coco::sys {
// TODO: Add support for IPv6
struct Ipv4Addr {
  constexpr Ipv4Addr() noexcept = default;
  constexpr Ipv4Addr(std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d) noexcept : mAddr{a, b, c, d} {}
  Ipv4Addr(std::uint32_t addr) noexcept : mAddr{*reinterpret_cast<std::uint8_t const*>(&addr)} {}
  operator std::uint32_t() const noexcept { return *reinterpret_cast<std::uint32_t const*>(mAddr); }

private:
  std::uint8_t mAddr[4];
};

struct SocketAddrV4 {
  constexpr SocketAddrV4() noexcept = default;
  constexpr SocketAddrV4(Ipv4Addr addr, std::uint16_t port) noexcept : mAddr{addr}, mPort{port} {}

  constexpr static auto localhost(std::uint16_t port) noexcept -> SocketAddrV4
  {
    return SocketAddrV4{Ipv4Addr{127, 0, 0, 1}, port};
  }
  constexpr static auto any(std::uint16_t port) noexcept -> SocketAddrV4
  {
    return SocketAddrV4{Ipv4Addr{0, 0, 0, 0}, port};
  }
  constexpr static auto broadcast(std::uint16_t port) noexcept -> SocketAddrV4
  {
    return SocketAddrV4{Ipv4Addr{255, 255, 255, 255}, port};
  }

private:
  friend struct SocketAddr;

  Ipv4Addr mAddr;
  std::uint16_t mPort;
};

struct SocketAddr {
  SocketAddr(SocketAddrV4 addr) noexcept : mV4{addr} {}

  auto toV4() const noexcept -> sockaddr_in
  {
    auto addr = sockaddr_in{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(mV4.mPort);
    addr.sin_addr.s_addr = mV4.mAddr;
    return addr;
  }
  auto toV6() const noexcept -> sockaddr_in { assert(0); }
  auto domain() const noexcept -> int
  {
    return AF_INET; // TODO
  }
  auto type() const noexcept -> int
  {
    return SOCK_STREAM; // TODO
  }
  auto port() const noexcept -> std::uint16_t { return mV4.mPort; }

  SocketAddrV4 mV4;
};

} // namespace coco::sys