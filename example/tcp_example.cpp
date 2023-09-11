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

static coco::Runtime rt(coco::INL, 4);

auto server() -> coco::Task<int>
{
  using namespace coco::sys;
  ::puts("server start");
  auto [listener, errc] = TcpListener::bind(SocketAddr(SocketAddrV4::localhost(2333)));

  if (errc != std::errc{0}) {
    print("bind error", errc);
    co_return 1;
  }

  auto [stream, errc2] = co_await listener.accept();

  if (errc2 != std::errc{0}) {
    print("accept error", errc2);
    co_return 1;
  }
  ::puts("server accepted");

  std::array<char, 1024> buf{};
  while (true) {
    auto [n, errc2] = co_await stream.recv(std::as_writable_bytes(std::span(buf)));

    if (errc2 != std::errc(0)) {
      print("recv error", errc2);
      co_return 1;
    }

    if (n == 0) {
      break;
    }
    buf[n] = '\0';
    ::printf("server get: %s\n", buf.data());
  }
  ::puts("server end");
  co_return 0;
}

auto client() -> coco::Task<>
{
  using namespace coco::sys;
  ::puts("client start");
  auto [client, errc] = co_await TcpStream::connect(SocketAddr(SocketAddrV4::localhost(2333)));
  if (errc != std::errc(0)) {
    print("connect error", errc);
    co_return;
  }
  ::puts("client connected");
  for (int i = 0; i < 10; i++) {
    auto [n, errc2] = co_await client.send(std::as_bytes(std::span("hi server")));
    if (errc2 != std::errc(0)) {
      print("send error", errc2);
      co_return;
    }
    if (n == 0) {
      break;
    }
    co_await rt.sleepFor(std::chrono::seconds(1));
  }
  ::puts("client end");
  co_return;
}

auto main() -> int
{
  rt.block([]() -> coco::Task<> {
    ::puts("tcp example");
    auto s = rt.spawn(server());
    auto c = rt.spawn(client());

    co_await c.join();
    co_await s.join();
    ::puts("everything done");
    co_return;
  }());
}

auto connect() -> coco::Task<>
{
  using namespace coco::sys;
  ::puts("client start");
  auto [client, errc] = co_await TcpStream::connect(SocketAddr(SocketAddrV4::localhost(2333)), 1s);
  if (errc != std::errc(0)) {
    print("connect error", errc);
    co_return;
  }
  ::puts("client connected");
  auto buf = std::array<char, 1024>{};
  auto [n, err] = co_await client.recv(std::as_writable_bytes(std::span(buf)), 5s);
  if (err != std::errc(0)) {
    print("recv error", err);
    co_return;
  }
}

auto main1() -> int
{
  rt.block([]() -> coco::Task<> {
    ::puts("tcp example");
    auto c = rt.spawn(connect());

    co_await c.join();
    ::puts("everything done");
    co_return;
  }());
}