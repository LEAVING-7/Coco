# [WIP] Coco
A simple and easy to use C++ 20 coroutine library.

## Features
The following is basically completed, but there may still be some bugs. (issues and PRs are welcome)
- [x] `Channel`, `Mutex`
- [x] `TcpStream`, `TcpListener`
- [x] `MultiThreadExecutor`, `InlineExecutor`
- [x] `waitAll`, `sleepFor`, `sleepUntil`

## TODO
- [ ] `CondVar`, `RwLock`, `Latch`
- [ ] **File IO**
- [ ] `whenAny`

## Example
Simple Tcp server and client
```cpp
static coco::Runtime rt(coco::MT, 4);

auto server() -> coco::Task<int>
{
  using namespace coco::net;
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
  using namespace coco::net;
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
```
