#include "coco/mt_executor.hpp"

namespace coco {

IoUring::IoUring()
{
  if (auto r = ::io_uring_queue_init(kIoUringQueueSize, &mUring, 0); r != 0) {
    throw std::system_error(-r, std::system_category(), "create uring instance failed");
  }
  mEventFd = ::eventfd(0, 0);
  if (mEventFd < 0) {
    throw std::system_error(errno, std::system_category(), "create eventfd failed");
  }
  auto sqe = fetchSqe();
  ::io_uring_prep_poll_multishot(sqe, mEventFd, POLLIN);
  ::io_uring_submit(&mUring);
}
IoUring::~IoUring()
{
  auto sqe = fetchSqe();
  ::io_uring_prep_poll_remove(sqe, 0);
  ::io_uring_submit_and_wait(&mUring, 1);
  ::close(mEventFd);
  ::io_uring_queue_exit(&mUring);
}
auto IoUring::prepRecv(Token token, int fd, std::span<std::byte> buf, int flag) noexcept -> void
{
  auto sqe = fetchSqe();
  ::io_uring_prep_recv(sqe, fd, (void*)buf.data(), buf.size(), 0);
  ::io_uring_sqe_set_data(sqe, token);
}
auto IoUring::prepSend(Token token, int fd, std::span<std::byte const> buf, int flag) noexcept -> void
{
  auto sqe = fetchSqe();
  ::io_uring_prep_send(sqe, fd, (void const*)buf.data(), buf.size(), flag);
  ::io_uring_sqe_set_data(sqe, token);
}
auto IoUring::prepAccept(Token token, int fd, sockaddr* addr, socklen_t* addrlen, int flags) noexcept -> void
{
  auto sqe = fetchSqe();
  ::io_uring_prep_accept(sqe, fd, addr, addrlen, flags);
  ::io_uring_sqe_set_data(sqe, token);
}
auto IoUring::prepConnect(Token token, int fd, sockaddr* addr, socklen_t addrlen) noexcept -> void
{
  auto sqe = fetchSqe();
  ::io_uring_prep_connect(sqe, fd, addr, addrlen);
  ::io_uring_sqe_set_data(sqe, token);
}
auto IoUring::seen(io_uring_cqe* cqe) noexcept -> void { ::io_uring_cqe_seen(&mUring, cqe); }
auto IoUring::submitWait(int waitn) noexcept -> std::errc
{
  auto r = ::io_uring_submit_and_wait(&mUring, waitn);
  if (r < 0) {
    return std::errc(-r);
  }
  return std::errc(0);
}
auto IoUring::prepCancel(int fd) noexcept -> void
{
  auto sqe = fetchSqe();
  ::io_uring_prep_cancel_fd(sqe, fd, 0);
}
auto IoUring::prepCancel(Token token) noexcept -> void
{
  auto sqe = fetchSqe();
  ::io_uring_prep_cancel(sqe, token, 0);
  ::io_uring_sqe_set_data(sqe, token);
}
auto IoUring::prepClose(Token token, int fd) noexcept -> void
{
  auto sqe = fetchSqe();
  ::io_uring_prep_close(sqe, fd);
  ::io_uring_sqe_set_data(sqe, token);
}
auto IoUring::notify() noexcept -> void
{
  auto buf = std::uint64_t(0);
  auto r = ::write(mEventFd, &buf, sizeof(buf));
  assert(r);
}
auto IoUring::fetchSqe() -> io_uring_sqe*
{
  auto sqe = ::io_uring_get_sqe(&mUring);
  if (sqe == nullptr) {
    throw std::runtime_error("sqe full"); // TODO: better without exception.
  }
  return sqe;
}
auto IoUring::advance(std::uint32_t n) noexcept -> void { ::io_uring_cq_advance(&mUring, n); }
auto IoUring::submit() noexcept -> std::errc
{
  auto r = ::io_uring_submit(&mUring);
  if (r < 0) {
    return std::errc(-r);
  }
  return std::errc(0);
}
} // namespace coco
