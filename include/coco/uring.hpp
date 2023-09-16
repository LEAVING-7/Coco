#pragma once

#include <liburing.h>
#include <sys/eventfd.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <chrono>
#include <coroutine>

#if !IO_URING_CHECK_VERSION(2, 4)
  #error "current liburing version is not supported"
#endif

namespace coco {
class Worker;
class WorkerJob;
class MtExecutor;

constexpr std::uint32_t kIoUringQueueSize = 2048;
using Token = void*;

template <typename Rep, typename Ratio>
static auto convertTime(std::chrono::duration<Rep, Ratio> duration, struct __kernel_timespec& out) noexcept -> void
{
  auto sec = std::chrono::duration_cast<std::chrono::seconds>(duration);
  auto nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - sec);
  out.tv_sec = sec.count();
  out.tv_nsec = nsec.count();
}

class IoUring {
public:
  IoUring();
  ~IoUring();

  IoUring(IoUring&& other) = delete;
  auto operator=(IoUring&& other) -> IoUring& = delete;

  auto prepRecv(Token token, int fd, std::span<std::byte> buf, int flag = 0) noexcept -> void;
  auto prepSend(Token token, int fd, std::span<std::byte const> buf, int flag = 0) noexcept -> void;
  auto prepRecvMsg(Token token, int fd, ::msghdr* msg, unsigned flag = 0) noexcept -> void;
  auto prepSendMsg(Token token, int fd, ::msghdr* msg, unsigned flag = 0) noexcept -> void;
  auto prepAccept(Token token, int fd, sockaddr* addr, socklen_t* addrlen, int flags = 0) noexcept -> void;
  auto prepAcceptMt(Token token, int fd, sockaddr* addr, socklen_t* addrlen, int flags = 0) noexcept -> void;
  auto prepConnect(Token token, int fd, sockaddr* addr, socklen_t addrlen) noexcept -> void;

  auto prepRead(Token token, int fd, std::span<std::byte> buf, off_t offset) noexcept -> void;
  auto prepWrite(Token token, int fd, std::span<std::byte const> buf, off_t offset) noexcept -> void;
  template <typename Rep, typename Ratio>
  auto prepAddTimeout(Token token, std::chrono::duration<Rep, Ratio> timeout) noexcept -> void
  {
    auto timeoutSpec = __kernel_timespec{};
    convertTime(timeout, timeoutSpec);
    auto sqe = fetchSqe();
    ::io_uring_prep_timeout(sqe, &timeoutSpec, 0, 0);
    ::io_uring_sqe_set_data(sqe, token);
  }
  template <typename Rep, typename Ratio>
  auto prepUpdateTimeout(Token token, std::chrono::duration<Rep, Ratio> timeout) noexcept -> void
  {
    auto timeoutSpec = __kernel_timespec{};
    convertTime(timeout, timeoutSpec);
    auto sqe = fetchSqe();
    ::io_uring_prep_timeout(sqe, &timeoutSpec, 1, 0);
    ::io_uring_sqe_set_data(sqe, token);
  }
  auto prepRemoveTimeout(Token token) noexcept -> void
  {
    auto sqe = fetchSqe();
    ::io_uring_prep_timeout_remove(sqe, (std::uint64_t)token, 0);
    ::io_uring_sqe_set_data(sqe, token);
  }
  auto prepCancel(int fd) noexcept -> void;
  auto prepCancel(Token token) noexcept -> void;
  auto prepClose(Token token, int fd) noexcept -> void;

  auto seen(io_uring_cqe* cqe) noexcept -> void;
  auto advance(std::uint32_t n) noexcept -> void;
  auto submitWait(int waitn) noexcept -> std::errc;
  auto submit() noexcept -> std::errc;

  template <typename Rep, typename Ratio>
  auto submitWait(io_uring_cqe*& cqe, std::chrono::duration<Rep, Ratio> duration) noexcept -> std::errc
  {
    auto timeout = __kernel_timespec{};
    convertTime(duration, timeout);
    auto r = ::io_uring_submit_and_wait_timeout(&mUring, &cqe, 1, &timeout, 0);
    return r < 0 ? std::errc(-r) : std::errc(0);
  }
  template <typename Rep, typename Ratio>
  auto submitWait(std::span<io_uring_cqe*> cqes, std::chrono::duration<Rep, Ratio> duration) noexcept -> std::errc
  {
    auto timeout = __kernel_timespec{};
    convertTime(duration, timeout);
    auto r = ::io_uring_submit_and_wait_timeout(&mUring, cqes.data(), cqes.size(), &timeout, 0);
    return r < 0 ? std::errc(-r) : std::errc(0);
  }

  // TODO: I can't find a method to notify a uring without a real fd :(.
  auto notify() noexcept -> void;
  auto uring() -> ::io_uring* { return &mUring; }

private:
  auto fetchSqe() -> io_uring_sqe*;

private:
  int mEventFd;
  ::io_uring mUring;
};
} // namespace coco