#include <coco/net.hpp>
#include <coco/runtime.hpp>

static coco::Runtime rt(10);

auto print(std::errc errc) -> void
{
  if (errc == std::errc{0}) {
    ::puts("success");
  } else {
    ::puts(std::generic_category().message(static_cast<int>(errc)).c_str());
  }
}

auto main() -> int
{
  rt.block([]() -> coco::Task<int> {
    using namespace coco::net;
    auto [listener, errc] = TcpListener::bind(SocketAddr(SocketAddrV4::localhost(2333)));
    if (errc != std::errc{0}) {
      print(errc);
      co_return 1;
    }
    auto [stream, errc2] = co_await listener.accept();
    if (errc2 != std::errc{0}) {
      print(errc2);
      co_return 1;
    }
    auto [n, errc3] = co_await stream.send(std::as_bytes(std::span("hello world")));
    if (errc3 != std::errc{0}) {
      print(errc3);
      co_return 1;
    }
    co_return 0;
  });
}
