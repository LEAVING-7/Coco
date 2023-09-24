#include <coco/net.hpp>
#include <coco/runtime.hpp>
#include <coco/sync/latch.hpp>

#include <string_view>
using namespace std::literals;

auto print(std::string_view msg, std::errc errc) -> void
{
  if (errc == std::errc{0}) {
    ::printf("%s: success\n", msg.data());
  } else {
    ::printf("%s: %s\n", msg.data(), std::generic_category().message(static_cast<int>(errc)).c_str());
  }
}

auto main() -> int
{
  static auto rt = coco::Runtime(coco::MT, 4);
  rt.block([]() -> coco::Task<> {
    using namespace coco::sys;
    ::puts("server start");
    auto addr = SocketAddr(SocketAddrV4::loopback(2333));
    auto [listener, errc] = TcpListener::bind(addr);

    if (errc != std::errc{0}) {
      print("bind error", errc);
      co_return;
    }

    static auto latch = coco::sync::Latch(10000000);
    for (int i = 0; i < 10000000; i++) {
      auto [stream, errc0] = co_await listener.accept();

      if (errc0 != std::errc{0}) {
        print("accept error", errc0);
        co_return;
      }

      rt.spawnDetach([](TcpStream stream, int i) -> coco::Task<> {
        std::array<char, 1024> buf{};
        auto [n, errc2] = co_await stream.recv(std::as_writable_bytes(std::span(buf)));

        if (errc2 != std::errc(0)) {
          print("recv error", errc2);
          co_return;
        } else {
          if (i % 1000 == 0) {
            ::printf("recv: %lu, id: %d\n", n, i);
          }
        }

        std::string str;
        str.reserve(1024 + 128);
        const auto http200 = "HTTP/1.1 200 OK\r\nContent-Length: 1024\r\nConnection: close\r\n\r\n"sv;
        str.append(http200);
        str.append(1024, 'a');
        str.append("\n");
        auto [n2, err3] = co_await stream.send(std::as_bytes(std::span(str)));
        if (errc2 != std::errc(0)) {
          print("recv error", errc2);
          co_return;
        }
        latch.countDown();
      }(std::move(stream), i));
    }
    co_await latch.wait();
    ::puts("server end");
    co_return;
  }());
}