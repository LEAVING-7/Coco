#include "mt_executor.hpp"

namespace coco {

UringInstance::UringInstance()
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
UringInstance::~UringInstance()
{
  auto sqe = fetchSqe();
  ::io_uring_prep_poll_remove(sqe, 0);
  ::io_uring_submit_and_wait(&mUring, 1);
  ::close(mEventFd);
  ::io_uring_queue_exit(&mUring);
}
auto UringInstance::prepRecv(Token token, int fd, BufSlice buf, int flag) -> void
{
  auto sqe = fetchSqe();
  ::io_uring_prep_recv(sqe, fd, (void*)buf.data(), buf.size(), 0);
  ::io_uring_sqe_set_data(sqe, token);
}
auto UringInstance::prepSend(Token token, int fd, BufView buf, int flag) -> void
{
  auto sqe = fetchSqe();
  ::io_uring_prep_send(sqe, fd, (void const*)buf.data(), buf.size(), flag);
  ::io_uring_sqe_set_data(sqe, token);
}
auto UringInstance::prepAccept(Token token, int fd, sockaddr* addr, socklen_t* addrlen, int flags) -> void
{
  auto sqe = fetchSqe();
  ::io_uring_prep_accept(sqe, fd, addr, addrlen, flags);
  ::io_uring_sqe_set_data(sqe, token);
}
auto UringInstance::seen(io_uring_cqe* cqe) -> void { ::io_uring_cqe_seen(&mUring, cqe); }
auto UringInstance::submitWait(int waitn) -> int
{
  auto r = ::io_uring_submit_and_wait(&mUring, waitn);
  if (r < 0) {
    throw std::system_error(-r, std::system_category(), "submit and wait failed");
  }
  return r;
}
auto UringInstance::prepCancel(int fd) -> void
{
  auto sqe = fetchSqe();
  ::io_uring_prep_cancel_fd(sqe, fd, 0);
}
auto UringInstance::prepCancel(Token token) -> void
{
  auto sqe = fetchSqe();
  ::io_uring_prep_cancel(sqe, token, 0);
  ::io_uring_sqe_set_data(sqe, token);
}
auto UringInstance::prepClose(Token token, int fd) -> void
{
  auto sqe = fetchSqe();
  ::io_uring_prep_close(sqe, fd);
  ::io_uring_sqe_set_data(sqe, token);
}
auto UringInstance::notify() -> void
{
  auto buf = std::uint64_t(0);
  auto r = ::write(mEventFd, &buf, sizeof(buf));
  assert(r);
}
auto UringInstance::fetchSqe() -> io_uring_sqe*
{
  auto sqe = ::io_uring_get_sqe(&mUring);
  if (sqe == nullptr) {
    throw std::runtime_error("sqe full");
  }
  return sqe;
}

auto UringInstance::execute(coco::WorkerJob* job) noexcept -> void { mMtExecutor->enqueue(job); }
auto UringInstance::execute(std::coroutine_handle<> handle) -> void
{
  auto job = new coco::CoroJob(handle);
  execute(job);
}

} // namespace coco
