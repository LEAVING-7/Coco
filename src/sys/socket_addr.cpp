#include "coco/sys/socket_addr.hpp"

namespace coco::sys {
auto Ipv4Addr::to_string() const -> std::string
{
  constexpr char max[] = "255.255.255.255";
  std::string result;
  result.reserve(sizeof(max));
  std::format_to_n(std::back_inserter(result), sizeof(max), "{}", *this);
  return result;
}

auto Ipv6Addr::to_string() const -> std::string
{
  constexpr char max[] = "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff";
  std::string result;
  result.reserve(sizeof(max));
  std::format_to_n(std::back_inserter(result), sizeof(max), "{}", *this);
  return result;
}

auto SocketAddrV4::to_string() const -> std::string
{
  constexpr char max[] = "255.255.255.255:65536";
  std::string result;
  result.reserve(sizeof(max));
  std::format_to_n(std::back_inserter(result), 22, "{}:{}", mAddr, mPort);
  return result;
}

auto SocketAddrV6::to_string() const -> std::string
{
  constexpr char max[] = "[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]:65536";
  std::string result;
  result.reserve(sizeof(max));
  std::format_to_n(std::back_inserter(result), 42, "[{}]:{}", mAddr, mPort);
  return result;
}

auto SocketAddr::to_string() const -> std::string { return std::format("{}", *this); }
} // namespace coco::sys