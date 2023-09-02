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
  auto value = rt.block([]() -> coco::Task<int> {
    using namespace coco::net;
    auto server = rt.spawn([]() -> coco::Task<> {
      // auto [listener, errc] = TcpListener::bind(SocketAddr(SocketAddrV4::localhost(2333)));
      // if (errc != std::errc{0}) {
      //   print(errc);
      //   co_return 1;
      // }
      // auto [stream, errc2] = co_await listener.accept();
      // if (errc2 != std::errc{0}) {
      //   print(errc2);
      //   co_return 1;
      // }

      // std::array<char, 1024> buf{};
      // while (true) {
      //   auto [n, errc2] = co_await stream.recv(std::as_writable_bytes(std::span(buf)));
      //   if (errc2 != std::errc(0)) {
      //     print(errc2);
      //     co_return 1;
      //   }
      //   if (n == 0) {
      //     break;
      //   }
      //   buf[n] = '\0';
      //   ::printf("get: %1024s\n", buf.data());
      // }

      co_return ;
    }());

    // auto client = rt.spawn([]() -> coco::Task<> {
    //   auto [client, errc] = co_await TcpStream::connect(SocketAddr(SocketAddrV4::localhost(2333)));
    //   if (errc != std::errc(0)) {
    //     print(errc);
    //     co_return;
    //   }
    //   for (int i = 0; i < 10; i++) {
    //     auto [n, errc2] = co_await client.send(std::as_bytes(std::span("hi server")));
    //     if (errc2 != std::errc(0)) {
    //       print(errc2);
    //       co_return;
    //     }
    //     if (n == 0) {
    //       break;
    //     }
    //     co_await rt.sleep(std::chrono::seconds(1));
    //   }
    //   co_return;
    // }());
    co_return 2333;
  });
  assert(value == 2333);
}
