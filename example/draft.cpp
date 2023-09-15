#include <coco/sys/socket_addr.hpp>
#include <iostream>



auto main() -> int
{
  using namespace coco::sys;
  Ipv6Addr ipv6{0, 0, 0, 0, 0, 0xffff, 0x7f00, 0x1};
  Ipv4Addr ipv4{127, 0, 0, 1};
  std::cout << std::format("{}", ipv6.to_string());
}