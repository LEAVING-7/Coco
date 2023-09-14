#include <coco/net.hpp>
#include <coco/runtime.hpp>
using namespace std::literals;

auto print(std::string_view msg, std::errc errc) -> void
{
  if (errc == std::errc{0}) {
    ::printf("%s: success\n", msg.data());
  } else {
    ::printf("%s: %s\n", msg.data(), std::generic_category().message(static_cast<int>(errc)).c_str());
  }
}

static coco::Runtime rt(coco::MT, 4);

auto server(coco::sys::SocketAddr addr) -> coco::Task<>
{
  using namespace coco::sys;
  ::puts("server start");

  auto [listener, errc] = UdpSocket::bind(addr);
  if (errc != std::errc{0}) {
    print("bind error", errc);
    co_return;
  } else {
    ::puts("server bind success");
  }

  std::array<char, 1024> buf{};
  for (int i = 0; i < 10; i++) {
    auto [n, errc] = co_await listener.recvfrom(std::as_writable_bytes(std::span(buf)), addr);

    if (errc != std::errc(0)) {
      print("recv error", errc);
      co_return;
    }

    buf[n] = '\0';
    ::printf("server get: %s\n", buf.data());
  }
  co_return;
}

auto client(coco::sys::SocketAddr addr) -> coco::Task<>
{
  using namespace coco::sys;
  ::puts("client start");
  auto [client, errc] = UdpSocket::create(addr);
  if (errc != std::errc(0)) {
    print("create error", errc);
    co_return;
  }

  for (int i = 0; i < 10; i++) {
    auto [n, errc] = co_await client.sendto(std::as_bytes(std::span("hello world")), addr);
    if (errc != std::errc(0)) {
      print("send error", errc);
      co_return;
    }
  }
}

auto main() -> int
{
  rt.block([]() -> coco::Task<> {
    using namespace coco::sys;
    ::puts("udp example");
    auto addr = SocketAddr(SocketAddrV4::localhost(2333));
    auto st = rt.spawn(server(addr));
    auto ct = rt.spawn(client(addr));

    co_await st.join();
    co_await ct.join();
    ::puts("everything done");
    co_return;
  }());
}