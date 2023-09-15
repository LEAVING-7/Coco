#pragma once
#include "coco/util/panic.hpp"

#include <netinet/in.h>
#include <sys/socket.h>

namespace coco::sys {
struct Ipv4Addr {
  constexpr Ipv4Addr() noexcept = default;
  constexpr Ipv4Addr(std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d) noexcept : mAddr{a, b, c, d} {}
  Ipv4Addr(std::uint32_t addr) noexcept : mAddr{*reinterpret_cast<std::uint8_t const*>(&addr)} {}
  operator std::uint32_t() const noexcept { return *reinterpret_cast<std::uint32_t const*>(mAddr); }

  constexpr auto isUnspecified() const noexcept -> bool { return *this == Ipv4Addr{}; }
  constexpr auto isLoopback() const noexcept -> bool { return mAddr[0] == 127; }

  constexpr static auto unspecified() noexcept -> Ipv4Addr { return Ipv4Addr{0, 0, 0, 0}; }
  constexpr static auto loopback() noexcept -> Ipv4Addr { return Ipv4Addr{127, 0, 0, 1}; }
  constexpr static auto broadcast() noexcept -> Ipv4Addr { return Ipv4Addr{255, 255, 255, 255}; }

  auto to_string() const -> std::string;
  auto operator==(Ipv4Addr const& other) const noexcept -> bool = default;

  std::uint8_t mAddr[4];
};

struct Ipv6Addr {
  constexpr Ipv6Addr() noexcept = default;
  constexpr Ipv6Addr(std::uint16_t a, std::uint16_t b, std::uint16_t c, std::uint16_t d, std::uint16_t e,
                     std::uint16_t f, std::uint16_t g, std::uint16_t h) noexcept
  {
    // to big endian
    mAddr[0] = a >> 8;
    mAddr[1] = a & 0xFF;
    mAddr[2] = b >> 8;
    mAddr[3] = b & 0xFF;
    mAddr[4] = c >> 8;
    mAddr[5] = c & 0xFF;
    mAddr[6] = d >> 8;
    mAddr[7] = d & 0xFF;
    mAddr[8] = e >> 8;
    mAddr[9] = e & 0xFF;
    mAddr[10] = f >> 8;
    mAddr[11] = f & 0xFF;
    mAddr[12] = g >> 8;
    mAddr[13] = g & 0xFF;
    mAddr[14] = h >> 8;
    mAddr[15] = h & 0xFF;
  }

  constexpr auto isUnspecified() const noexcept -> bool { return *this == Ipv6Addr::unspecified(); }
  constexpr auto isLoopback() const noexcept -> bool { return *this == Ipv6Addr::loopback(); }
  // TODO: isMulticast
  constexpr static auto unspecified() noexcept -> Ipv6Addr { return Ipv6Addr{0, 0, 0, 0, 0, 0, 0, 0}; }
  constexpr static auto loopback() noexcept -> Ipv6Addr { return Ipv6Addr{0, 0, 0, 0, 0, 0, 0, 1}; }

  constexpr auto toSegments(std::span<std::uint16_t, 8> dst) const noexcept -> void
  {
    for (std::size_t i = 0; i < dst.size(); i++) {
      dst[i] = mAddr[i * 2] << 8 | mAddr[i * 2 + 1];
    }
  }

  auto to_string() const -> std::string;
  auto operator==(Ipv6Addr const& other) const noexcept -> bool = default;

  std::uint8_t mAddr[16];
};

struct SocketAddrV4 {
  constexpr SocketAddrV4() noexcept = default;
  constexpr SocketAddrV4(Ipv4Addr addr, std::uint16_t port) noexcept : mAddr{addr}, mPort{port} {}
  constexpr auto ip() noexcept -> Ipv4Addr& { return mAddr; }
  constexpr auto port() noexcept -> std::uint16_t { return mPort; }

  constexpr static auto unspecified(std::uint16_t port) noexcept -> SocketAddrV4
  {
    return {Ipv4Addr::unspecified(), port};
  }
  constexpr static auto loopback(std::uint16_t port) noexcept -> SocketAddrV4 { return {Ipv4Addr::loopback(), port}; }
  constexpr static auto broadcast(std::uint16_t port) noexcept -> SocketAddrV4 { return {Ipv4Addr::broadcast(), port}; }

  auto to_string() const -> std::string;
  auto operator==(SocketAddrV4 const& other) const noexcept -> bool = default;

  Ipv4Addr mAddr;
  std::uint16_t mPort;
};

struct SocketAddrV6 {
  constexpr SocketAddrV6() noexcept = default;
  constexpr SocketAddrV6(Ipv6Addr addr, std::uint16_t port, std::uint32_t flowInfo, std::uint32_t scopeId) noexcept
      : mAddr{addr}, mPort{port}, mFlowInfo{flowInfo}, mScopeId{scopeId}
  {
  }
  constexpr auto ip() noexcept -> Ipv6Addr& { return mAddr; }
  constexpr auto port() noexcept -> std::uint16_t { return mPort; }
  constexpr auto flowInfo() noexcept -> std::uint32_t { return mFlowInfo; }
  constexpr auto scopeId() noexcept -> std::uint32_t { return mScopeId; }

  constexpr static auto loopback(std::uint16_t port, std::uint32_t flowInfo = 0, std::uint32_t scopeId = 0) noexcept
      -> SocketAddrV6
  {
    return {Ipv6Addr::loopback(), port, flowInfo, scopeId};
  }
  constexpr static auto unspecified(std::uint16_t port, std::uint32_t flowInfo = 0, std::uint32_t scopeId = 0) noexcept
      -> SocketAddrV6
  {
    return {Ipv6Addr::unspecified(), port, flowInfo, scopeId};
  }

  auto to_string() const -> std::string;
  auto operator==(SocketAddrV6 const& other) const noexcept -> bool = default;

  Ipv6Addr mAddr;
  std::uint16_t mPort;
  std::uint32_t mFlowInfo;
  std::uint32_t mScopeId;
};

struct SocketAddr {
  SocketAddr(SocketAddrV4 addr) noexcept : mV4{addr}, mKind(V4) {}
  SocketAddr(SocketAddrV6 addr) noexcept : mV6{addr}, mKind(V6) {}

  auto domain() const noexcept -> int { return mKind == V4 ? AF_INET : AF_INET6; }
  auto port() const noexcept -> std::uint16_t { return mKind == V4 ? mV4.mPort : mV6.mPort; }
  auto setPort(std::uint16_t port) noexcept -> void
  {
    if (mKind == V4) {
      mV4.mPort = port;
    } else {
      mV6.mPort = port;
    }
  }
  auto isIpv4() const noexcept -> bool { return mKind == V4; }
  auto isIpv6() const noexcept -> bool { return mKind == V6; }
  auto setIp(Ipv4Addr const& addr) noexcept -> void
  {
    assert(mKind == V4);
    mV4.mAddr = addr;
  }
  auto setIp(Ipv6Addr const& addr) noexcept -> void
  {
    assert(mKind == V6);
    mV6.mAddr = addr;
  }

  auto setSys(sockaddr_in& out) const -> void
  {
    out.sin_family = AF_INET;
    out.sin_port = htons(port());
    out.sin_addr.s_addr = mV4.mAddr;
  }
  auto setSys(sockaddr_in6& out) const -> void
  {
    out.sin6_family = AF_INET6;
    out.sin6_port = htons(port());
    out.sin6_flowinfo = mV6.mFlowInfo;
    out.sin6_scope_id = mV6.mScopeId;
    std::memcpy(&out.sin6_addr, mV6.mAddr.mAddr, sizeof(out.sin6_addr));
  }
  auto to_string() const -> std::string;

private:
  friend struct std::formatter<SocketAddr>;

  union {
    SocketAddrV4 mV4;
    SocketAddrV6 mV6;
  };
  enum { V4, V6 } mKind;
};
} // namespace coco::sys

template <>
struct std::formatter<coco::sys::Ipv4Addr> {
  constexpr auto parse(auto& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(coco::sys::Ipv4Addr const& addr, FormatContext& ctx) const
  {
    return std::format_to(ctx.out(), "{}.{}.{}.{}", addr.mAddr[0], addr.mAddr[1], addr.mAddr[2], addr.mAddr[3]);
  }
};

template <>
struct std::formatter<coco::sys::Ipv6Addr> {
  constexpr auto parse(auto& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(coco::sys::Ipv6Addr const& addr, FormatContext& ctx) const
  {
    if (addr.isUnspecified()) {
      return std::format_to(ctx.out(), "::");
    } else if (addr.isLoopback()) {
      return std::format_to(ctx.out(), "::1");
    } else {
      std::array<std::uint16_t, 8> segments;
      addr.toSegments(segments);
      struct Span {
        std::uint32_t start;
        std::uint32_t len;
      };

      Span longest{0, 0};
      Span current{0, 0};
      for (std::size_t i = 0; i < segments.size(); i++) {
        if (segments[i] == 0) {
          if (current.len == 0) {
            current.start = i;
          }
          current.len++;

          if (current.len > longest.len) {
            longest = current;
          }
        } else {
          current = {};
        }
      }
      Span zeros = longest;
      auto out = ctx.out();
      if (zeros.len > 1) {
        for (std::size_t i = 0; i < zeros.start; i++) {
          if (i == 0) {
            out = std::format_to(ctx.out(), "{:x}", segments[i]);
          } else {
            out = std::format_to(ctx.out(), ":{:x}", segments[i]);
          }
        }
        std::format_to(ctx.out(), "::");
        for (std::size_t i = zeros.start + zeros.len; i < segments.size(); i++) {
          if (i == zeros.start + zeros.len) {
            out = std::format_to(ctx.out(), "{:x}", segments[i]);
          } else {
            out = std::format_to(ctx.out(), ":{:x}", segments[i]);
          }
        }
      } else {
        for (std::size_t i = 0; i < segments.size(); i++) {
          if (i == 0) {
            out = std::format_to(ctx.out(), "{:x}", segments[i]);
          } else {
            out = std::format_to(ctx.out(), ":{:x}", segments[i]);
          }
        }
      }
      return out;
    }
  }
};

template <>
struct std::formatter<coco::sys::SocketAddrV4> {
  constexpr auto parse(auto& ctx) { return ctx.begin(); }
  auto format(coco::sys::SocketAddrV4 const& addr, auto& ctx) const
  {
    return std::format_to(ctx.out(), "{}:{}", addr.mAddr, addr.mPort);
  }
};

template <>
struct std::formatter<coco::sys::SocketAddrV6> {
  constexpr auto parse(auto& ctx) { return ctx.begin(); }
  auto format(coco::sys::SocketAddrV6 const& addr, auto& ctx) const
  {
    if (addr.mFlowInfo == 0) {
      return std::format_to(ctx.out(), "[{}]:{}", addr.mAddr, addr.mPort);
    } else {
      return std::format_to(ctx.out(), "[{}%{}]:{}", addr.mAddr, addr.mScopeId, addr.mPort);
    }
  }
};

template <>
struct std::formatter<coco::sys::SocketAddr> {
  constexpr auto parse(auto& ctx) { return ctx.begin(); }
  auto format(coco::sys::SocketAddr const& addr, auto& ctx) const
  {
    if (addr.isIpv4()) {
      return std::format_to(ctx.out(), "{}", addr.mV4);
    } else {
      return std::format_to(ctx.out(), "{}", addr.mV6);
    }
  }
};